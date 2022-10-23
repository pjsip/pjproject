extern crate bindgen;

pub fn main() {
    println!("cargo:rerun-if-changed=../../../pjsip/include/pjsua.h");
    println!("cargo:rerun-if-changed=build.rs");

    println!("cargo:rustc-link-search=../../../pjsip/lib");
    println!("cargo:rustc-link-search=../../../pjlib/lib");
    println!("cargo:rustc-link-search=../../../pjlib-util/lib");
    println!("cargo:rustc-link-search=../../../pjmedia/lib");
    println!("cargo:rustc-link-search=../../../pjnath/lib");
    println!("cargo:rustc-link-search=../../../third_party/lib");
    println!("cargo:rustc-link-search=../../../lib");

    println!("cargo:rustc-link-lib=pj");
    println!("cargo:rustc-link-lib=pjsip-simple");
    println!("cargo:rustc-link-lib=pjsip-ua");
    println!("cargo:rustc-link-lib=pjsip");
    println!("cargo:rustc-link-lib=pjsua");
    println!("cargo:rustc-link-lib=pjlib-util");
    println!("cargo:rustc-link-lib=pjmedia-audiodev");
    println!("cargo:rustc-link-lib=pjmedia-codec");
    println!("cargo:rustc-link-lib=pjmedia-videodev");
    println!("cargo:rustc-link-lib=pjmedia");
    println!("cargo:rustc-link-lib=pjnath");

    println!("cargo:rustc-link-lib=g7221codec");
    println!("cargo:rustc-link-lib=gsmcodec");
    println!("cargo:rustc-link-lib=ilbccodec");
    println!("cargo:rustc-link-lib=resample");
    println!("cargo:rustc-link-lib=speex");
    println!("cargo:rustc-link-lib=srtp");
    println!("cargo:rustc-link-lib=webrtc");
    println!("cargo:rustc-link-lib=yuv");

    
    let bindings = bindgen::Builder::default()
        .header("../../../pjsip/include/pjsua.h")
        .clang_args(["-I", "../../../pjsip/include"])
        .clang_args(["-I", "../../../pjlib/include"])
        .clang_args(["-I", "../../../pjlib-util/include"])
        .clang_args(["-I", "../../../pjmedia/include"])
        .clang_args(["-I", "../../../pjnath/include"])
        .clang_args(["-I", "../../../include"])
        .generate_comments(false)
        .allowlist_type(r"pj.*")
        .allowlist_type(r"PJ.*")
        .allowlist_var(r"pj.*")
        .allowlist_var(r"PJ.*")
        .allowlist_function(r"pj.*")
        .allowlist_function(r"PJ.*")
        .generate()
        .expect("Couldn't generate bindings!");

    let out_path = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());

    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
