use std::path::Path;

fn main() {
    let source_directory = Path::new("src");
    let parser_path = source_directory.join("parser.c");
    let scanner_path = source_directory.join("scanner.c");
    let mut c_config = cc::Build::new();
    c_config.std("c11").include(source_directory);

    #[cfg(target_env = "msvc")]
    c_config.flag("-utf-8");

    let target = std::env::var("TARGET").expect("Cargo must provide TARGET");
    if target == "wasm32-unknown-unknown" {
        let wasm_headers = std::env::var_os("DEP_TREE_SITTER_LANGUAGE_WASM_HEADERS")
            .map(std::path::PathBuf::from)
            .expect(
                "tree-sitter-language must provide its headers when compiling for wasm32-unknown-unknown",
            );
        c_config.include(wasm_headers);
    }

    c_config.file(&parser_path).file(&scanner_path);
    c_config.compile("tree-sitter-sh");

    println!("cargo:rerun-if-changed={}", parser_path.display());
    println!("cargo:rerun-if-changed={}", scanner_path.display());
}
