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

const budgets = {
  STATE_COUNT: 22_500,
  LARGE_STATE_COUNT: 1_500,
  SYMBOL_COUNT: 600,
  EXTERNAL_TOKEN_COUNT: 100,
  parser_bytes: 30_000_000,
  maximum_ACTIONS_index: 35_000,
  parse_table_storage_bytes: 3_500_000,
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

function maximumActionIndex(parser) {
  let maximum;
  for (const match of parser.matchAll(/ACTIONS\(([0-9]+)\)/g)) {
    const value = Number(match[1]);
    maximum = maximum === undefined ? value : Math.max(maximum, value);
  }
  if (maximum === undefined) {
    throw new Error("Generated parser.c contains no ACTIONS index");
  }
  return maximum;
}

function smallParseTableWordCount(parser) {
  const declaration = "static const uint16_t ts_small_parse_table[] = {\n";
  const start = parser.indexOf(declaration);
  if (start === -1) {
    throw new Error("Generated parser.c contains no small parse table");
  }
  const initializerStart = start + declaration.length;
  const initializerEnd = parser.indexOf("\n};", initializerStart);
  if (initializerEnd === -1) {
    throw new Error("Generated parser.c has an unterminated small parse table");
  }
  const initializer = parser.slice(initializerStart, initializerEnd);

  let finalIndex;
  let finalOffset;
  for (const match of initializer.matchAll(/^ {2}\[([0-9]+)\] =/gm)) {
    finalIndex = Number(match[1]);
    finalOffset = match.index;
  }
  if (finalIndex === undefined || finalOffset === undefined) {
    throw new Error("Generated parser.c has no indexed small parse table row");
  }

  const finalRowWordCount =
    initializer.slice(finalOffset).match(/,/g)?.length ?? 0;
  if (finalRowWordCount === 0) {
    throw new Error(
      "Generated parser.c has an empty final small parse table row",
    );
  }
  return finalIndex + finalRowWordCount;
}

function parseTableStorageBytes(parser, metrics) {
  const smallStateCount = metrics.STATE_COUNT - metrics.LARGE_STATE_COUNT;
  if (smallStateCount < 0) {
    throw new Error(
      "Generated parser.c has more large states than total states",
    );
  }
  return (
    metrics.LARGE_STATE_COUNT * metrics.SYMBOL_COUNT * 2 +
    smallParseTableWordCount(parser) * 2 +
    smallStateCount * 4
  );
}

const temporaryDirectory = createEnvironmentDirectory(
  "tree-sitter-sh-generated",
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

  const nodeTypes = JSON.parse(
    fs.readFileSync(path.join(generatedDirectory, "node-types.json"), "utf8"),
  );
  const bracketRange = nodeTypes.find(
    (nodeType) => nodeType.type === "pattern_bracket_range_source",
  );
  for (const field of ["start", "end"]) {
    const cardinality = bracketRange?.fields?.[field];
    if (cardinality?.required !== true || cardinality.multiple !== false) {
      process.stderr.write(
        `Generated pattern bracket range ${field} field is not singular and required\n`,
      );
      failed = true;
    }
  }

  const parser = fs.readFileSync(
    path.join(generatedDirectory, "parser.c"),
    "utf8",
  );
  try {
    const parserPath = path.join(generatedDirectory, "parser.c");
    const metrics = {
      STATE_COUNT: readDefinition(parser, "STATE_COUNT"),
      LARGE_STATE_COUNT: readDefinition(parser, "LARGE_STATE_COUNT"),
      SYMBOL_COUNT: readDefinition(parser, "SYMBOL_COUNT"),
      EXTERNAL_TOKEN_COUNT: readDefinition(parser, "EXTERNAL_TOKEN_COUNT"),
      parser_bytes: fs.statSync(parserPath).size,
      maximum_ACTIONS_index: maximumActionIndex(parser),
    };
    metrics.parse_table_storage_bytes = parseTableStorageBytes(parser, metrics);

    console.log("Metric                     Actual      Maximum");
    for (const [name, maximum] of Object.entries(budgets)) {
      console.log(
        `${name.padEnd(22)} ${String(metrics[name]).padStart(12)} ${String(maximum).padStart(12)}`,
      );
      if (metrics[name] > maximum) {
        process.stderr.write(
          `Generated ${name} budget exceeded: ${metrics[name]} > ${maximum}\n`,
        );
        failed = true;
      }
    }
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    failed = true;
  }
} finally {
  fs.rmSync(temporaryDirectory, { force: true, recursive: true });
}

if (failed) {
  process.exitCode = 1;
}
