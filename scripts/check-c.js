import { spawnSync } from "node:child_process";
import {
  accessSync,
  constants,
  mkdtempSync,
  readFileSync,
  realpathSync,
  rmSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { basename, delimiter, dirname, isAbsolute, join } from "node:path";
import { grammars, packageName, root } from "./tree-sitter.js";

const scannerConfigurations = {
  sh: {
    externalCount: "TOKEN_COUNT",
    enumerators: {
      _io_number_begin: "FILE_DESCRIPTOR",
      _bang_begin: "PIPELINE_NEGATION",
      _if_keyword_begin: "IF_KEYWORD",
      _then_keyword_begin: "THEN_KEYWORD",
      _elif_keyword_begin: "ELIF_KEYWORD",
      _else_keyword_begin: "ELSE_KEYWORD",
      _fi_keyword_begin: "FI_KEYWORD",
      _for_keyword_begin: "FOR_KEYWORD",
      _in_keyword_begin: "IN_KEYWORD",
      _do_keyword_begin: "DO_KEYWORD",
      _done_keyword_begin: "DONE_KEYWORD",
      _case_keyword_begin: "CASE_KEYWORD",
      _esac_keyword_begin: "ESAC_KEYWORD",
      _while_keyword_begin: "WHILE_KEYWORD",
      _until_keyword_begin: "UNTIL_KEYWORD",
      _dless_commit: "DLESS",
      _dlessdash_commit: "DLESSDASH",
      _here_document_line_end_begin: "HERE_DOCUMENT_LINE_END",
      _here_document_end_line_end_begin: "HERE_DOCUMENT_END_LINE_END",
      _quoted_here_document_end_text_begin: "QUOTED_HERE_DOCUMENT_END_TEXT",
      _quoted_here_document_text_begin: "QUOTED_HERE_DOCUMENT_TEXT",
      _here_document_end_leading_tabs_begin: "HERE_DOCUMENT_END_LEADING_TABS",
      _newline_begin: "NEWLINE",
      _comment_text_begin: "COMMENT_START",
      _comment_line_end_begin: "COMMENT_LINE_END",
      _braced_positional_parameter_begin: "BRACED_POSITIONAL_PARAMETER_START",
      _pattern_bracket_character_begin: "PATTERN_BRACKET_CHARACTER",
      _parameter_pattern_bracket_character_begin:
        "PARAMETER_PATTERN_BRACKET_CHARACTER",
      _pattern_bracket_hyphen_begin: "PATTERN_BRACKET_HYPHEN",
      _assignment_name_begin: "ASSIGNMENT_NAME_TOKEN",
      _fname_begin: "FNAME_TOKEN",
      _pre_newline_blank_begin: "PRE_NEWLINE_BLANK",
      _dollar_single_quote_escape_begin: "DOLLAR_SINGLE_QUOTE_ESCAPE",
      "\\": "CONTINUATION",
    },
    reuseAllocator: true,
  },
};

const warningArguments = ["-Wall", "-Wextra", "-Werror", "-pedantic"];
const scannerContract = join(root, "test", "scanner.test.c");
const contracts = [scannerContract, join(root, "test", "source.test.c")];

function run(
  command,
  arguments_,
  { stdio = "inherit", timeout = 60_000 } = {},
) {
  const result = spawnSync(command, arguments_, {
    cwd: root,
    encoding: "utf8",
    timeout,
    killSignal: "SIGKILL",
    maxBuffer: 64 * 1024 * 1024,
    stdio,
  });
  if (result.error) {
    throw new Error(
      `${command} ${arguments_.join(" ")}: ${result.error.message}`,
      { cause: result.error },
    );
  }
  if (result.status !== 0) {
    const diagnostics = (result.stderr || result.stdout || "").trim();
    throw new Error(
      `${command} ${arguments_.join(" ")} failed with status ${result.status ?? 1}${diagnostics ? `\n${diagnostics}` : ""}`,
    );
  }
  return result;
}

function executableCandidates(name) {
  if (process.platform !== "win32") return [name];
  const extensions = (process.env.PATHEXT ?? ".EXE;.CMD;.BAT")
    .split(";")
    .filter(Boolean)
    .map((extension) => extension.toLowerCase());
  if (extensions.some((extension) => name.toLowerCase().endsWith(extension)))
    return [name];
  return [name, ...extensions.map((extension) => name + extension)];
}

function isExecutable(path) {
  try {
    accessSync(path, constants.X_OK);
    return statSync(path).isFile();
  } catch {
    return false;
  }
}

function findExecutable(name, directories) {
  if (isAbsolute(name) || name.includes("/") || name.includes("\\")) {
    if (isExecutable(name)) return name;
    throw new Error(`Cannot execute ${name}.`);
  }
  for (const directory of directories) {
    for (const candidate of executableCandidates(name)) {
      const path = join(directory, candidate);
      if (isExecutable(path)) return path;
    }
  }
  throw new Error(`Cannot find ${name} on PATH.`);
}

function versionMajor(command) {
  const version = run(command, ["--version"], { stdio: "pipe" }).stdout;
  const match = version.match(/version ([0-9]+)/);
  if (match === null)
    throw new Error(`Cannot determine the version of ${command}.`);
  return Number(match[1]);
}

function llvmCommands() {
  if (process.platform === "darwin") {
    const prefix = run("brew", ["--prefix", "llvm"], {
      stdio: "pipe",
    }).stdout.trim();
    const directory = join(prefix, "bin");
    return {
      clang: findExecutable(join(directory, "clang"), []),
      clangd: findExecutable(join(directory, "clangd"), []),
      clangFormat: findExecutable(join(directory, "clang-format"), []),
    };
  }
  const directories = (process.env.PATH ?? "").split(delimiter);
  const clangFormat = findExecutable(
    process.env.CLANG_FORMAT ?? "clang-format",
    directories,
  );
  const directory = dirname(realpathSync(clangFormat));
  return {
    clang: findExecutable(
      process.env.CLANG ?? "clang",
      process.env.CLANG === undefined ? [directory] : directories,
    ),
    clangd: findExecutable(
      process.env.CLANGD ?? "clangd",
      process.env.CLANGD === undefined ? [directory] : directories,
    ),
    clangFormat,
  };
}

function scannerVariants() {
  return grammars.map((grammar) => {
    const configuration = scannerConfigurations[grammar.name];
    if (configuration === undefined)
      throw new Error(`Unsupported scanner grammar ${grammar.name}.`);
    if (typeof configuration.reuseAllocator !== "boolean")
      throw new Error(
        `Scanner grammar ${grammar.name} must declare reuseAllocator.`,
      );
    const includeDirectory = join(root, grammar.path, "src");
    return {
      ...configuration,
      name: grammar.name,
      includeDirectory,
      source: join(includeDirectory, "scanner.c"),
      headers: grammar.externalFiles
        .filter((file) => file.endsWith(".h"))
        .map((file) => join(root, file)),
    };
  });
}

function checkExternalTokenOrder(clang, compilerArguments, variant, directory) {
  const grammar = JSON.parse(
    readFileSync(join(variant.includeDirectory, "grammar.json"), "utf8"),
  );
  const assertions = grammar.externals.map((external, index) => {
    const name = external.type === "SYMBOL" ? external.name : external.value;
    const enumerator =
      variant.enumerators?.[name] ??
      (external.type === "SYMBOL"
        ? name.replace(/^_/, "").toUpperCase()
        : undefined);
    if (enumerator === undefined) {
      throw new Error(
        `Missing scanner enumerator for ${JSON.stringify(external)}.`,
      );
    }
    return `typedef char external_${index}[${enumerator} == ${index} ? 1 : -1];`;
  });
  assertions.push(
    `typedef char external_count[(${variant.externalCount}) == ${grammar.externals.length} ? 1 : -1];`,
  );
  const source = join(directory, `scanner-indices-${variant.name}.c`);
  writeFileSync(
    source,
    `#include ${JSON.stringify(variant.source.replaceAll("\\", "/"))}\n${assertions.join("\n")}\n`,
  );
  run(clang, [...compilerArguments, "-fsyntax-only", source]);
}

function checkDiagnostics(clangd, sources) {
  for (const source of sources) {
    process.stdout.write(`clangd: ${source}\n`);
    // --check also probes editor features at every token in large files.
    run(clangd, ["--log=error", "--tweaks=", `--check=${source}`], {
      timeout: 180_000,
    });
  }
}

function checkAllocatorSymbols(clang, variant, directory) {
  const object = join(directory, `scanner-reuse-${variant.name}.o`);
  run(clang, [
    "-std=c17",
    ...warningArguments,
    "-I",
    variant.includeDirectory,
    "-DTREE_SITTER_REUSE_ALLOCATOR",
    "-c",
    variant.source,
    "-o",
    object,
  ]);
  const symbols = new Set(
    run("nm", ["-u", object], { stdio: "pipe" })
      .stdout.split("\n")
      .map((line) => line.trim().split(/ +/).at(-1)?.replace(/^_/, "")),
  );
  const allocators = ["malloc", "calloc", "realloc", "free"];
  if (allocators.some((allocator) => symbols.has(allocator)))
    throw new Error("Scanner bypasses the Tree-sitter allocator.");
  if (!allocators.some((allocator) => symbols.has(`ts_current_${allocator}`)))
    throw new Error("Scanner does not reference the Tree-sitter allocator.");
}

function main(arguments_) {
  if (
    arguments_.length > 1 ||
    (arguments_.length === 1 &&
      !["--write", "--sanitize"].includes(arguments_[0]))
  ) {
    throw new Error("Usage: node scripts/check-c.js [--write | --sanitize]");
  }
  const variants = scannerVariants();
  const sources = [
    ...new Set([
      ...variants.flatMap((variant) => variant.headers),
      ...variants.map((variant) => variant.source),
      ...contracts,
    ]),
  ];
  const { clang, clangd, clangFormat } = llvmCommands();
  if (new Set([clang, clangd, clangFormat].map(versionMajor)).size !== 1)
    throw new Error(
      "clang, clangd, and clang-format must use the same LLVM major version.",
    );

  if (arguments_[0] === "--write") {
    run(clangFormat, ["-i", ...sources]);
    return;
  }
  const sanitizerArguments =
    arguments_[0] === "--sanitize"
      ? [
          "-fsanitize=address,undefined",
          "-fno-sanitize-recover=undefined",
          "-fno-omit-frame-pointer",
          "-g",
        ]
      : [];
  const directory = mkdtempSync(join(tmpdir(), `${packageName}-scanner-`));
  try {
    checkDiagnostics(clangd, sources);
    run(clangFormat, ["--dry-run", "--Werror", ...sources]);
    for (const standard of ["c99", "c17"]) {
      for (const variant of variants) {
        const compilerArguments = [
          ...sanitizerArguments,
          `-std=${standard}`,
          ...warningArguments,
          "-I",
          variant.includeDirectory,
        ];
        checkExternalTokenOrder(clang, compilerArguments, variant, directory);
        for (const contract of contracts) {
          const modes =
            contract === scannerContract && variant.reuseAllocator
              ? [false, true]
              : [false];
          for (const reuse of modes) {
            const suffix = process.platform === "win32" ? ".exe" : "";
            const contractName = basename(contract, ".test.c");
            const name = `${variant.name}-${contractName}-${standard}${reuse ? "-reuse" : ""}`;
            const binary = join(directory, `scanner-${name}${suffix}`);
            run(clang, [
              ...compilerArguments,
              ...(variant.contractArguments ?? []),
              ...(reuse ? ["-DTREE_SITTER_REUSE_ALLOCATOR"] : []),
              contract,
              "-o",
              binary,
            ]);
            run(binary, []);
            process.stdout.write(
              `${variant.name}: ${contractName} tests passed (${standard.toUpperCase()}${reuse ? ", reused allocator" : ""})\n`,
            );
          }
        }
        if (standard === "c17" && variant.reuseAllocator)
          checkAllocatorSymbols(clang, variant, directory);
      }
    }
  } finally {
    rmSync(directory, { force: true, recursive: true });
  }
}

try {
  main(process.argv.slice(2));
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
