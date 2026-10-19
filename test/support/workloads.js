export function scalingWorkloads() {
  const continuedComment = (count) => {
    const slash = "\\";
    const prefix = `: "\`: ${slash}"${slash}\`#x`;
    const pair = `${slash.repeat(3)}\n${slash.repeat(4)}\n`;
    return `${prefix}${pair.repeat(count)}world\n${slash}\`${slash}"\`"\n`;
  };
  const encodedWordlist = (count) => {
    let source = `for x in value${"\\\n".repeat(count)} ; do :; done`;
    const backquoteDepth = 3;
    for (let depth = 0; depth < backquoteDepth; depth += 1) {
      source = `: \`${source.replaceAll("\\", "\\\\").replaceAll("`", "\\`")}\``;
    }
    return `${source}\n`;
  };
  return [
    {
      name: "nested comment continuation parsing",
      units: 1_500,
      source: (count) => continuedComment(count),
    },
    {
      name: "backquote wordlist continuation parsing",
      units: 3_000,
      source: (count) => encodedWordlist(count),
    },
    {
      name: "spaced line-continuation parsing",
      units: 3_000,
      source: (count) => `printf value${" \\\n".repeat(count)}after\n`,
    },
    {
      name: "direct line-continuation parsing",
      units: 3_000,
      source: (count) => `printf value ${"\\\n".repeat(count)}after\n`,
    },
    {
      name: "arithmetic layout parsing",
      units: 2_000,
      source: (count) => `: "$((a${" \\\n".repeat(count)}+ b))"\n`,
    },
    {
      name: "dynamic arithmetic layout parsing",
      units: 2_000,
      source: (count) => `: "$((1${" \\\n".repeat(count)}$operator 2))"\n`,
    },
    {
      name: "bracket suffix parsing",
      units: 3_000,
      source: (count) => `printf ${'[a"x"*[.]'.repeat(count)}\n`,
    },
    {
      name: "bracket prefix and suffix parsing",
      units: 3_000,
      source: (count) => `printf ${"[x[:y]z[a]".repeat(count)}\n`,
    },
    {
      name: "chained bracket range parsing",
      units: 8_192,
      source: (count) => `printf [${"a-".repeat(count)}a]\n`,
    },
  ];
}

export function* editingWorkloads() {
  const commandLine = "echo aaaa\n";
  for (const lineCount of [1200, 2400, 4800]) {
    for (const row of [0, Math.floor(lineCount / 2), lineCount - 1]) {
      yield {
        name: `commands/${lineCount}/${row}`,
        source: commandLine.repeat(lineCount),
        edit: {
          byte: commandLine.length * row + commandLine.indexOf("aaaa"),
          deleteBytes: 1,
          insert: "b",
        },
        reuse: true,
      };
    }
  }
  const functionLine = "alpha() { : beta; }\n";
  const functionRows = 1200;
  for (const row of [0, Math.floor(functionRows / 2), functionRows - 1]) {
    yield {
      name: `function closer/${row}`,
      source: functionLine.repeat(functionRows),
      edit: {
        byte: functionLine.length * row + functionLine.indexOf("}"),
        deleteBytes: 1,
        insert: "",
      },
      recovery: true,
    };
  }
  const commentRows = 4800;
  const comments = "# comment\n".repeat(commentRows);
  yield {
    name: "final comment",
    source: comments,
    edit: { byte: comments.length - 2, deleteBytes: 1, insert: "x" },
  };
  const argumentCount = 1200;
  const arguments_ = " x=~/bin:$PATH".repeat(argumentCount);
  for (const [name, prefix, edit] of [
    [
      "declaration-name",
      "export",
      { byte: 0, deleteBytes: 6, insert: "printf" },
    ],
    [
      "command-option",
      "command export",
      { byte: 8, deleteBytes: 0, insert: "-- " },
    ],
  ])
    yield { name, source: `${prefix}${arguments_}\n`, edit };
  for (const leaf of ["1", `\${x:-1}`]) {
    for (const depth of [16, 24, 32]) {
      const prefix = `echo ${"$((".repeat(depth)}`;
      yield {
        name: `nested arithmetic/${leaf}/${depth}`,
        source: `${prefix}${leaf}${"))".repeat(depth)}\n`,
        edit: {
          byte: prefix.length + leaf.indexOf("1"),
          deleteBytes: 1,
          insert: "2",
        },
        counts: [["arithmetic_expansion", depth]],
      };
    }
  }
  const fragment = ` \${x} 1`;
  for (const fixture of [
    {
      name: "viable dynamic arithmetic",
      source: (count, finalNumber) =>
        `echo $((1${fragment.repeat(count - 1)} \${x} ${finalNumber}))\n`,
      arithmeticCount: 1,
      commandCount: 0,
      extraParameters: 0,
    },
    {
      name: "global assignment fallback",
      source: (count, finalNumber) =>
        `echo $((1${fragment.repeat(count)} \${x} a = ${finalNumber}))\n`,
      arithmeticCount: 0,
      commandCount: 1,
      extraParameters: 1,
    },
    {
      name: "spaced runtime fragment fallback",
      source: (count, finalNumber) =>
        `echo $((1${` \${x} `.repeat(count)}= ${finalNumber}))\n`,
      arithmeticCount: 0,
      commandCount: 1,
      extraParameters: 0,
    },
  ]) {
    for (const count of [64, 96, 128]) {
      const source = fixture.source(count, "1");
      yield {
        name: `${fixture.name}/${count}`,
        source,
        edit: { byte: source.lastIndexOf("1"), deleteBytes: 1, insert: "2" },
        counts: [
          ["arithmetic_expansion", fixture.arithmeticCount],
          ["arithmetic_dynamic_expression", fixture.arithmeticCount],
          ["command_substitution\n", fixture.commandCount],
          ["parameter_expansion", count + fixture.extraParameters],
        ],
      };
    }
  }
}
