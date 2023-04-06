extern crate bindgen;
extern crate pkg_config;

pub fn main() {
    let conf = pkg_config::probe_library("libpjproject").expect("libpjproject not found!");

    let mut clang_args = Vec::new();

    // get the include paths from pkg-config and create a clang argument for each
    for path in conf.include_paths {
        clang_args.push("-I".to_string());
        clang_args.push(
            path.to_str()
                .expect(&format!("Couldn't convert PathBuf: {:?} to str!", path))
                .to_string(),
        );
    }

    // get the define flags from pkg-config and create a clang argument for each
    for define in conf.defines {
        if let Some(value) = define.1 {
            clang_args.push(format!("-D{}={}", define.0, value));
        }
    }

    let bindings = bindgen::Builder::default()
        .header("../../../../pjsip/include/pjsua.h")
        .clang_args(clang_args)
        .generate_comments(false)
        .allowlist_type(r"pj.*")
        .allowlist_type(r"PJ.*")
        .allowlist_var(r"pj.*")
        .allowlist_var(r"PJ.*")
        .allowlist_function(r"pj.*")
        .allowlist_function(r"PJ.*")
        .generate()
        .expect("Couldn't generate bindings!");

    let out_path = std::path::PathBuf::from(std::env::var("OUT_DIR").expect("OUT_DIR not set!"));

    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
