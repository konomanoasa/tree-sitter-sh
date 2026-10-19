import assert from "node:assert/strict";
import { test } from "node:test";
import { grammars } from "../scripts/tree-sitter.js";
import {
  applyEdits,
  incrementalParseArguments,
  parserLibrary,
  runParserCommand,
  writeSource,
} from "./support/parser.js";
import { editingWorkloads, scalingWorkloads } from "./support/workloads.js";

const measurementSamples = 3;
const warmupSamples = 1;
const scalingInputMultipliers = [1, 2, 4];
const maximumDurationMultiplier = 8;
const scalingSlackMs = 100;

function medianDuration(samples) {
  const sorted = [...samples].sort((left, right) => left - right);
  return sorted[(sorted.length - 1) >> 1];
}

function readDuration(stdout, label, description) {
  const value = Number(
    stdout.match(new RegExp(`${label}:[ ]+([0-9.]+) ms`))?.[1],
  );
  assert.ok(
    Number.isFinite(value),
    `${description}: ${label} timing is unreadable\n${stdout}`,
  );
  return value;
}

function medianOperationDuration(arguments_, label, description, mode) {
  const samples = [];
  for (let run = 0; run < measurementSamples + warmupSamples; run += 1) {
    const result = runParserCommand(
      arguments_,
      mode === "recovery" ? [0, 1] : [0],
    );
    samples.push(readDuration(result.stdout, label, description));
  }
  return medianDuration(samples.slice(warmupSamples));
}

function medianFreshParseDuration(source, description, mode = "valid") {
  return medianOperationDuration(
    [
      "parse",
      "--lib-path",
      parserLibrary,
      "--lang-name",
      grammars[0].name,
      "--time",
      "--quiet",
      "--",
      source,
    ],
    "Parse",
    description,
    mode,
  );
}

function medianIncrementalParseDuration(
  source,
  edit,
  description,
  mode = "valid",
) {
  return medianOperationDuration(
    incrementalParseArguments(source, edit, "--time", "--quiet"),
    "Edit",
    description,
    mode,
  );
}

for (const { name, units, source } of scalingWorkloads()) {
  test(`sh: ${name} stays within the growth budget`, (context) => {
    const durations = scalingInputMultipliers.map((factor) =>
      medianFreshParseDuration(
        writeSource(`${name}-${factor}`, source(units * factor)),
        name,
      ),
    );
    context.diagnostic(`${name}: ${durations.join(" / ")}ms`);
    for (const duration of durations.slice(1)) {
      assert.ok(
        duration <= durations[0] * maximumDurationMultiplier + scalingSlackMs,
        name,
      );
    }
  });
}
for (const { name, source: contents, edit, recovery } of editingWorkloads()) {
  test(`sh: editing timings for ${name}`, (context) => {
    const source = writeSource(`${name}-initial`, contents);
    const final = writeSource(
      `${name}-edited`,
      applyEdits(Buffer.from(contents), [edit]),
    );
    const mode = recovery ? "recovery" : "valid";
    const incremental = medianIncrementalParseDuration(
      source,
      edit,
      name,
      mode,
    );
    const fresh = medianFreshParseDuration(final, name, mode);
    context.diagnostic(
      `${name}: incremental ${incremental}ms, fresh ${fresh}ms`,
    );
  });
}
