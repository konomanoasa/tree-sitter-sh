const fs = require("node:fs");
const path = require("node:path");

const {
  createEnvironmentDirectory,
  grammarDirectory,
  runTreeSitter,
} = require("./tree-sitter");

const generatedFiles = [
  "src/grammar.json",
  "src/node-types.json",
  "src/parser.c",
  "src/tree_sitter/alloc.h",
  "src/tree_sitter/array.h",
  "src/tree_sitter/parser.h",
];

const limits = {
  EXTERNAL_TOKEN_COUNT: 122,
  STATE_COUNT: 33_500,
  parserSize: 46_000_000,
};

function readDefinition(source, name) {
  const prefix = `#define ${name} `;
  const line = source
    .split("\n")
    .find((candidate) => candidate.startsWith(prefix));
  const value = line?.slice(prefix.length).trim() ?? "";
  if (!/^[0-9]+$/.test(value)) {
    throw new Error(`Could not read ${name} from generated parser.c`);
  }
  return Number(value);
}

const temporaryDirectory = createEnvironmentDirectory(
  "tree-sitter-posix-sh-generated",
);
const generatedDirectory = path.join(temporaryDirectory, "output");
const environmentDirectory = path.join(temporaryDirectory, "environment");
let failed = false;

try {
  fs.mkdirSync(generatedDirectory);
  fs.mkdirSync(environmentDirectory);
  runTreeSitter(
    [
      "generate",
      "--output",
      generatedDirectory,
      path.join(grammarDirectory, "grammar.js"),
    ],
    { environmentDirectory },
  );

  const pendingDirectories = [generatedDirectory];
  const manifest = [];
  while (pendingDirectories.length > 0) {
    const directory = pendingDirectories.pop();
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
      const entryPath = path.join(directory, entry.name);
      if (entry.isDirectory()) {
        pendingDirectories.push(entryPath);
      } else {
        manifest.push(
          path
            .relative(generatedDirectory, entryPath)
            .split(path.sep)
            .join("/"),
        );
      }
    }
  }
  manifest.sort();
  const expectedManifest = generatedFiles
    .map((generatedFile) => generatedFile.slice("src/".length))
    .sort();
  if (JSON.stringify(manifest) !== JSON.stringify(expectedManifest)) {
    process.stderr.write(
      `Generated file manifest differs:\nexpected ${expectedManifest.join(", ")}\nactual   ${manifest.join(", ")}\n`,
    );
    failed = true;
  }

  for (const generatedFile of generatedFiles) {
    const generatedOutput = path.join(
      generatedDirectory,
      generatedFile.slice("src/".length),
    );
    const trackedOutput = path.join(grammarDirectory, generatedFile);
    if (
      !fs.existsSync(generatedOutput) ||
      !fs.existsSync(trackedOutput) ||
      !fs.readFileSync(generatedOutput).equals(fs.readFileSync(trackedOutput))
    ) {
      process.stderr.write(`Generated file is stale: ${generatedFile}\n`);
      failed = true;
    }
  }

  const parser = fs.readFileSync(
    path.join(generatedDirectory, "parser.c"),
    "utf8",
  );
  for (const name of ["STATE_COUNT", "EXTERNAL_TOKEN_COUNT"]) {
    try {
      const value = readDefinition(parser, name);
      if (value > limits[name]) {
        const budget =
          name === "STATE_COUNT" ? "parser state" : "external token";
        process.stderr.write(
          `Generated ${budget} budget exceeded: ${value} > ${limits[name]}\n`,
        );
        failed = true;
      }
    } catch (error) {
      process.stderr.write(`${error.message}\n`);
      failed = true;
    }
  }

  const parserSize = fs.statSync(
    path.join(generatedDirectory, "parser.c"),
  ).size;
  if (parserSize > limits.parserSize) {
    process.stderr.write(
      `Generated parser size budget exceeded: ${parserSize} > ${limits.parserSize} bytes\n`,
    );
    failed = true;
  }
} finally {
  fs.rmSync(temporaryDirectory, { force: true, recursive: true });
}

if (failed) {
  process.exitCode = 1;
}
