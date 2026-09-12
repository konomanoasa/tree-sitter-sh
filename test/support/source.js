import assert from "node:assert/strict";

function applyEdits(source, edits) {
  let bytes = Buffer.from(source);
  for (const edit of edits) {
    const { byte, deleteBytes, insert } = edit;
    const description = JSON.stringify(edit);
    assert.ok(
      Number.isSafeInteger(byte) && byte >= 0,
      `invalid byte offset: ${description}`,
    );
    assert.ok(
      Number.isSafeInteger(deleteBytes) && deleteBytes >= 0,
      `invalid deletion length: ${description}`,
    );
    assert.equal(typeof insert, "string", `invalid insertion: ${description}`);
    assert.ok(
      byte <= bytes.length && deleteBytes <= bytes.length - byte,
      `edit exceeds ${bytes.length} source bytes: ${description}`,
    );
    bytes = Buffer.concat([
      bytes.subarray(0, byte),
      Buffer.from(insert),
      bytes.subarray(byte + deleteBytes),
    ]);
  }
  return bytes;
}

function formatEdit({ byte, deleteBytes, insert }) {
  return `${byte} ${deleteBytes} ${insert}`;
}

function sourceEndPoint(source) {
  const bytes = Buffer.from(source);
  let row = 0;
  for (const byte of bytes) if (byte === 10) row += 1;
  return `${row}:${bytes.length - bytes.lastIndexOf(10) - 1}`;
}

function createEditHistoryGenerator() {
  let seed = 1n;
  function next(maximum) {
    seed = BigInt.asUintN(64, seed * 6364136223846793005n + 1n);
    return Number(seed >> 32n) % maximum;
  }
  return function* (fragments, insertions, joinSource) {
    for (let iteration = 0; iteration < 100; iteration++) {
      const parts = [];
      const count = 1 + next(3);
      for (let part = 0; part < count; part++) {
        parts.push(fragments[next(fragments.length)]);
      }
      const initial = joinSource(parts);
      let source = Buffer.from(initial);
      const edits = [];
      for (let step = 0; step < 5; step++) {
        const position = next(source.length + 1);
        const insert = next(2) === 0 || position === source.length;
        const edit = insert
          ? {
              byte: position,
              deleteBytes: 0,
              insert: insertions[next(insertions.length)],
            }
          : {
              byte: position,
              deleteBytes: Math.min(next(2) + 1, source.length - position),
              insert: "",
            };
        edits.push(edit);
        source = applyEdits(source, [edit]);
        yield {
          initial,
          source,
          edits: [...edits],
          context: `seed 1, iteration ${iteration}, source ${JSON.stringify(initial)}, edits ${JSON.stringify(edits)}`,
        };
      }
    }
  };
}

export { applyEdits, createEditHistoryGenerator, formatEdit, sourceEndPoint };
