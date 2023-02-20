use std::env;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::path::Path;

fn main() {
	println!("cargo:rerun-if-changed=../subtitle_maker/subtitles.csv");
	let subtitles = std::fs::read_to_string("../subtitle_maker/subtitles.csv").unwrap();

	let mut map: phf_codegen::Map<&str> = phf_codegen::Map::new();
	for line in subtitles.lines() {
		let parts: Vec<&str> = line.split(';').collect();
		map.entry(
			parts[1],
			&format!(
				"Subtitle {{ duration: {}, subtitle: c_str!(\"{}\"), name: c_str!(\"{}\") }}",
				parts[0], parts[2], parts[1]
			),
		);
	}

	let path = Path::new(&env::var("OUT_DIR").unwrap()).join("codegen.rs");
	let mut file = BufWriter::new(File::create(&path).unwrap());

	write!(
		&mut file,
		"static SUBTITLES: phf::Map<&'static str, Subtitle> = {};\n",
		map.build()
	)
	.unwrap();
}
