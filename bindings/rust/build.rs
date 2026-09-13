fn main() {
    println!("cargo::rerun-if-changed=src");

    cc::Build::new()
        .std("c17")
        .include("src")
        .files(["src/parser.c", "src/scanner.c"])
        .compile("tree-sitter-sh");
}
