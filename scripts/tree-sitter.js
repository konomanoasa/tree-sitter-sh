const childProcess = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

const repositoryDirectory = path.resolve(__dirname, "..");
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
const cacheDirectory = path.join(
  repositoryDirectory,
  "node_modules/.cache/tree-sitter-sh",
);
const treeSitterPackageDirectory = path.dirname(
  require.resolve("tree-sitter-cli/package.json"),
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
  fs.mkdirSync(cacheDirectory, { recursive: true });
  fs.mkdirSync(treeSitterConfigurationDirectory, { recursive: true });
  fs.mkdirSync(libraryDirectory, { recursive: true });
  fs.writeFileSync(
    path.join(treeSitterConfigurationDirectory, "config.json"),
    `${JSON.stringify(
      { "parser-directories": [path.dirname(grammarDirectory)] },
      null,
      2,
    )}\n`,
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

if (require.main === module) {
  try {
    const result = runTreeSitter(process.argv.slice(2), { stdio: "inherit" });
    process.exitCode = result.status;
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  }
}

module.exports = {
  createEnvironmentDirectory,
  environmentFor,
  grammarDirectory,
  grammarName: grammar.name,
  repositoryDirectory,
  runTreeSitter,
  treeSitterExecutable,
};
