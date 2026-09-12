import childProcess from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const repositoryDirectory = path.resolve(import.meta.dirname, "..");
const configuration = JSON.parse(
  fs.readFileSync(path.join(repositoryDirectory, "tree-sitter.json"), "utf8"),
);

if (
  !Array.isArray(configuration.grammars) ||
  configuration.grammars.length !== 1
) {
  throw new Error("tree-sitter.json must define exactly one grammar");
}

const grammar = configuration.grammars[0];
const grammarPath = grammar?.path ?? ".";
if (
  grammar === null ||
  typeof grammar !== "object" ||
  typeof grammar.name !== "string" ||
  grammar.name.length === 0 ||
  typeof grammar.scope !== "string" ||
  grammar.scope.length === 0 ||
  typeof grammarPath !== "string" ||
  grammarPath.length === 0
) {
  throw new Error("tree-sitter.json grammar metadata is incomplete");
}

const grammarDirectory = path.resolve(repositoryDirectory, grammarPath);
const grammarName = grammar.name;
const cacheDirectory = path.join(
  repositoryDirectory,
  "node_modules/.cache/tree-sitter-sh",
);
const treeSitterPackageDirectory = path.dirname(
  fileURLToPath(import.meta.resolve("tree-sitter-cli/package.json")),
);
const treeSitterExecutable = path.join(
  treeSitterPackageDirectory,
  process.platform === "win32" ? "tree-sitter.exe" : "tree-sitter",
);

function createEnvironmentDirectory(label) {
  return fs.mkdtempSync(path.join(os.tmpdir(), `${label}-`));
}

function environmentFor(directory, additions = {}) {
  const configurationDirectory = path.join(directory, "config");
  const treeSitterConfigurationDirectory = path.join(
    configurationDirectory,
    "tree-sitter",
  );
  const libraryDirectory = path.join(directory, "lib");
  // CLI discovery requires a tree-sitter-* entry regardless of checkout name.
  const parserDirectory = path.join(directory, "parsers");
  const parserLink = path.join(parserDirectory, `tree-sitter-${grammar.name}`);
  fs.mkdirSync(cacheDirectory, { recursive: true });
  fs.mkdirSync(treeSitterConfigurationDirectory, { recursive: true });
  fs.mkdirSync(libraryDirectory, { recursive: true });
  fs.mkdirSync(parserDirectory, { recursive: true });
  if (!fs.existsSync(parserLink)) {
    fs.symlinkSync(grammarDirectory, parserLink, "junction");
  }
  fs.writeFileSync(
    path.join(treeSitterConfigurationDirectory, "config.json"),
    `${JSON.stringify({ "parser-directories": [parserDirectory] }, null, 2)}\n`,
  );
  return {
    ...process.env,
    APPDATA: configurationDirectory,
    LOCALAPPDATA: cacheDirectory,
    NO_COLOR: "1",
    TREE_SITTER_DIR: treeSitterConfigurationDirectory,
    TREE_SITTER_LIBDIR: libraryDirectory,
    TREE_SITTER_SEED: process.env.TREE_SITTER_SEED ?? "1",
    XDG_CACHE_HOME: cacheDirectory,
    XDG_CONFIG_HOME: configurationDirectory,
    ...additions,
  };
}

function runTreeSitter(arguments_, options = {}) {
  const temporaryEnvironment = options.environmentDirectory === undefined;
  const environmentDirectory =
    options.environmentDirectory ??
    createEnvironmentDirectory("tree-sitter-sh");

  try {
    const result = childProcess.spawnSync(treeSitterExecutable, arguments_, {
      cwd: options.cwd ?? repositoryDirectory,
      encoding: options.encoding ?? "utf8",
      env: environmentFor(environmentDirectory, options.env),
      input: options.input,
      maxBuffer: options.maxBuffer ?? 256 * 1024 * 1024,
      stdio: options.stdio,
      timeout: options.timeout,
      killSignal: "SIGKILL",
    });

    if (result.error !== undefined) {
      throw result.error;
    }

    const allowedStatuses = options.allowedStatuses ?? [0];
    if (!allowedStatuses.includes(result.status)) {
      const diagnostics = result.stderr || result.stdout || "";
      throw new Error(
        `tree-sitter ${arguments_.join(" ")} failed with status ${result.status}\n${diagnostics}`,
      );
    }

    return result;
  } finally {
    if (temporaryEnvironment) {
      fs.rmSync(environmentDirectory, { force: true, recursive: true });
    }
  }
}

function fuzzParsers(arguments_) {
  const directory = createEnvironmentDirectory("tree-sitter-sh-fuzz");
  const library = path.join(
    directory,
    process.platform === "win32" ? "parser.dll" : "parser.so",
  );
  try {
    runTreeSitter(["build", grammarDirectory, "--output", library], {
      environmentDirectory: directory,
      stdio: "inherit",
    });
    const result = runTreeSitter(
      [
        "fuzz",
        "--lib-path",
        library,
        "--lang-name",
        grammarName,
        ...arguments_,
      ],
      {
        environmentDirectory: directory,
        allowedStatuses: [0, 1],
        timeout: 600_000,
      },
    );
    process.stdout.write(result.stdout);
    process.stderr.write(result.stderr);
    // The CLI can report failed fuzz cases while returning exit status zero.
    if (
      /^[1-9][0-9]* .+ corpus tests failed fuzzing$/m.test(
        result.stdout + result.stderr,
      )
    )
      return 1;
    return result.status;
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  try {
    const [command, ...rest] = process.argv.slice(2);
    process.exitCode =
      command === "fuzz-all"
        ? fuzzParsers(rest)
        : runTreeSitter(process.argv.slice(2), { stdio: "inherit" }).status;
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  }
}

export {
  createEnvironmentDirectory,
  environmentFor,
  grammarDirectory,
  grammarName,
  repositoryDirectory,
  runTreeSitter,
  treeSitterExecutable,
};
