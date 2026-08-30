const childProcess = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");

const {
  createEnvironmentDirectory,
  grammarDirectory,
  repositoryDirectory,
} = require("./tree-sitter");

const bindingIncludeDirectory = path.join(repositoryDirectory, "bindings", "c");
const bindingHeaderSource = path.join(
  bindingIncludeDirectory,
  "tree_sitter",
  "tree-sitter-sh.h",
);
const bindingContractSource = path.join(
  repositoryDirectory,
  "test/binding.test.c",
);
const scannerSource = path.join(grammarDirectory, "src/scanner.c");
const scannerContractSource = path.join(
  repositoryDirectory,
  "test/scanner.test.c",
);
const cSources = [
  bindingHeaderSource,
  bindingContractSource,
  scannerSource,
  scannerContractSource,
];

function run(command, arguments_, options = {}) {
  const result = childProcess.spawnSync(command, arguments_, {
    cwd: repositoryDirectory,
    encoding: "utf8",
    env: process.env,
    maxBuffer: 64 * 1024 * 1024,
    stdio: options.stdio,
  });
  if (result.error !== undefined) {
    throw result.error;
  }
  if (result.status !== 0) {
    const diagnostics = result.stderr || result.stdout || "";
    throw new Error(
      `${command} ${arguments_.join(" ")} failed with status ${result.status}\n${diagnostics}`,
    );
  }
  return result;
}

function resolveTools() {
  if (process.platform !== "darwin") {
    return {
      clang: process.env.CLANG ?? "clang",
      clangd: process.env.CLANGD ?? "clangd",
      clangFormat: process.env.CLANG_FORMAT ?? "clang-format",
    };
  }

  const llvmDirectory = path.join(
    run("brew", ["--prefix", "llvm"]).stdout.trim(),
    "bin",
  );
  return {
    clang: path.join(llvmDirectory, "clang"),
    clangd: path.join(llvmDirectory, "clangd"),
    clangFormat: path.join(llvmDirectory, "clang-format"),
  };
}

function undefinedSymbols(output) {
  const symbols = new Set();
  for (const line of output.split("\n")) {
    const fields = line.trim().split(" ").filter(Boolean);
    if (fields.length === 0) {
      continue;
    }
    const symbol = fields.at(-1);
    symbols.add(symbol.startsWith("_") ? symbol.slice(1) : symbol);
  }
  return symbols;
}

// The scanner names these tokens after their classification rather than
// their grammar spelling; every other enumerator is the grammar name
// uppercased without its leading underscore.
const externalEnumeratorExceptions = new Map([
  ["_io_number_token", "FILE_DESCRIPTOR"],
  ["_io_location_token", "IO_LOCATION"],
  ["_bang_token", "PIPELINE_NEGATION"],
  ["_dless_commit", "DLESS"],
  ["_dlessdash_commit", "DLESSDASH"],
  ["_pattern_bracket_character_token", "PATTERN_BRACKET_CHARACTER"],
  [
    "_parameter_pattern_bracket_character_token",
    "PARAMETER_PATTERN_BRACKET_CHARACTER",
  ],
  ["_pattern_bracket_hyphen_token", "PATTERN_BRACKET_HYPHEN"],
]);

// The external scanner reads tokens by enum position, so the TokenType
// enumerators must list every grammar external in declaration order,
// followed only by the TOKEN_COUNT sentinel.
function checkExternalTokenOrder(scannerSource) {
  const grammar = JSON.parse(
    fs.readFileSync(path.join(grammarDirectory, "src/grammar.json"), "utf8"),
  );
  const externals = grammar.externals.map((external) => external.name);

  const enumMatch = fs
    .readFileSync(scannerSource, "utf8")
    .match(/enum TokenType \{([^}]*)\}/);
  if (enumMatch === null) {
    throw new Error("Scanner does not declare enum TokenType");
  }
  const enumerators = enumMatch[1]
    .split("\n")
    .map((line) => line.trim())
    .filter((line) => line !== "" && !line.startsWith("//"))
    .map((line) => line.replace(/,.*$/, ""));

  if (enumerators.at(-1) !== "TOKEN_COUNT") {
    throw new Error("enum TokenType must end with the TOKEN_COUNT sentinel");
  }
  if (enumerators.length - 1 !== externals.length) {
    throw new Error(
      `enum TokenType has ${enumerators.length - 1} tokens for ${externals.length} grammar externals`,
    );
  }
  for (const [index, external] of externals.entries()) {
    const expected =
      externalEnumeratorExceptions.get(external) ??
      external.replace(/^_/, "").toUpperCase();
    if (enumerators[index] !== expected) {
      throw new Error(
        `enum TokenType position ${index} is ${enumerators[index]}, but the grammar external there is ${external} (expected ${expected})`,
      );
    }
  }
}

const temporaryDirectory = createEnvironmentDirectory("tree-sitter-sh-c");

try {
  const requestedArguments = process.argv.slice(2);
  if (
    requestedArguments.length > 1 ||
    (requestedArguments.length === 1 && requestedArguments[0] !== "--write")
  ) {
    throw new Error("Usage: node scripts/check-c.js [--write]");
  }

  const { clang, clangd, clangFormat } = resolveTools();

  if (requestedArguments[0] === "--write") {
    run(clangFormat, ["-i", ...cSources], {
      stdio: "inherit",
    });
  } else {
    run(clangFormat, ["--dry-run", "--Werror", ...cSources], {
      stdio: "inherit",
    });

    checkExternalTokenOrder(scannerSource);

    const scannerReuseObject = path.join(temporaryDirectory, "scanner-reuse.o");
    const warningArguments = ["-Wall", "-Wextra", "-Werror", "-pedantic"];
    const bindingArguments = [
      ...warningArguments,
      `-I${bindingIncludeDirectory}`,
    ];
    const scannerArguments = [
      ...warningArguments,
      `-I${path.join(grammarDirectory, "src")}`,
    ];

    const compileCommands = [
      {
        arguments: [
          clang,
          "-std=c17",
          "-xc",
          "-fsyntax-only",
          bindingHeaderSource,
        ],
        directory: repositoryDirectory,
        file: bindingHeaderSource,
      },
      {
        arguments: [
          clang,
          "-std=c17",
          ...bindingArguments,
          "-fsyntax-only",
          bindingContractSource,
        ],
        directory: repositoryDirectory,
        file: bindingContractSource,
      },
      {
        arguments: [
          clang,
          "-std=c17",
          ...scannerArguments,
          "-fsyntax-only",
          scannerSource,
        ],
        directory: repositoryDirectory,
        file: scannerSource,
      },
      {
        // The contract includes scanner.c to exercise private helpers. Clangd
        // otherwise reports unused helpers from that non-main file; the real
        // contract compilation below still treats every warning as an error.
        arguments: [
          clang,
          "-std=c17",
          ...scannerArguments,
          "-Wno-unused-function",
          "-fsyntax-only",
          scannerContractSource,
        ],
        directory: repositoryDirectory,
        file: scannerContractSource,
      },
    ];
    fs.writeFileSync(
      path.join(temporaryDirectory, "compile_commands.json"),
      `${JSON.stringify(compileCommands)}\n`,
    );

    for (const source of cSources) {
      run(
        clangd,
        [
          "--log=error",
          "--tweaks=",
          `--compile-commands-dir=${temporaryDirectory}`,
          `--check=${source}`,
        ],
        { stdio: "inherit" },
      );
    }

    for (const standard of ["c99", "c17"]) {
      const bindingStandardArguments = [
        `-std=${standard}`,
        ...bindingArguments,
      ];
      const scannerStandardArguments = [
        `-std=${standard}`,
        ...scannerArguments,
      ];
      const scannerContract = path.join(
        temporaryDirectory,
        `scanner-contract-${standard}`,
      );
      const scannerReuseContract = path.join(
        temporaryDirectory,
        `scanner-reuse-contract-${standard}`,
      );

      run(
        clang,
        [...bindingStandardArguments, "-fsyntax-only", bindingContractSource],
        { stdio: "inherit" },
      );
      run(
        clang,
        [...scannerStandardArguments, "-fsyntax-only", scannerSource],
        {
          stdio: "inherit",
        },
      );
      run(
        clang,
        [
          ...scannerStandardArguments,
          scannerContractSource,
          "-o",
          scannerContract,
        ],
        { stdio: "inherit" },
      );
      run(scannerContract, [], { stdio: "inherit" });
      run(
        clang,
        [
          ...scannerStandardArguments,
          "-DTREE_SITTER_REUSE_ALLOCATOR",
          scannerContractSource,
          "-o",
          scannerReuseContract,
        ],
        { stdio: "inherit" },
      );
      run(scannerReuseContract, [], { stdio: "inherit" });
    }

    run(
      clang,
      [
        "-std=c17",
        ...scannerArguments,
        "-DTREE_SITTER_REUSE_ALLOCATOR",
        "-c",
        scannerSource,
        "-o",
        scannerReuseObject,
      ],
      { stdio: "inherit" },
    );

    const symbols = undefinedSymbols(
      run("nm", ["-u", scannerReuseObject]).stdout,
    );
    const allocators = ["malloc", "calloc", "realloc", "free"];
    if (allocators.some((allocator) => symbols.has(allocator))) {
      throw new Error("Scanner bypasses the Tree-sitter allocator");
    }
    if (
      !allocators.some((allocator) => symbols.has(`ts_current_${allocator}`))
    ) {
      throw new Error("Scanner does not reference the Tree-sitter allocator");
    }
  }
} catch (error) {
  process.stderr.write(`${error.message}\n`);
  process.exitCode = 1;
} finally {
  fs.rmSync(temporaryDirectory, { force: true, recursive: true });
}
