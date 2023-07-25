#![feature(let_chains)]
use bevy::prelude::*;
use bevy_egui::*;
use std::io::Write;
use std::path::PathBuf;
use std::process::Command;

const BATCH_PATH: &'static str = "/path/to/batch";
const WHISPER_MODEL: &'static str = "medium.en";
const WHISPER_LANGUAGE: &'static str = "en";

fn main() {
	App::new()
		.add_plugins(DefaultPlugins)
		.add_plugins(EguiPlugin)
		.insert_resource(Files {
			files: vec![],
			cur_file: None,
		})
		.insert_resource(DataCollection { data: vec![] })
		.insert_resource(Complete {
			complete: false,
			completed: -1,
			total: 0,
		})
		.add_systems(Startup, load_files)
		.add_systems(Update, ui)
		.run();
}

fn load_files(
	mut files_res: ResMut<Files>,
	mut complete: ResMut<Complete>,
	asset_server: ResMut<AssetServer>,
) {
	let mut files = vec![];

	for file in std::fs::read_dir(BATCH_PATH).unwrap() {
		let path = file.unwrap().path();
		let handle = asset_server.load(path.clone());
		let name = path.file_name().unwrap().to_string_lossy();
		let name = name.strip_suffix(".hca.wav").unwrap();
		if name.chars().nth(name.len() - 2) == Some('_') {
			continue;
		}
		files.push(File {
			path,
			handle,
			initial_subtitle: None,
		});
	}

	generate_subtitle_ai(&mut files);
	files.sort_by(|a, b| {
		let a = a.path.file_name().unwrap();
		let b = b.path.file_name().unwrap();
		a.cmp(b)
	});
	files.reverse();

	complete.total = files.len();
	files_res.files = files;
	files_res.cur_file = None;
}

#[derive(Resource)]
struct Complete {
	complete: bool,
	completed: i32,
	total: usize,
}

#[derive(Resource)]
struct File {
	handle: Handle<AudioSource>,
	path: PathBuf,
	initial_subtitle: Option<String>,
}

#[derive(Resource)]
struct Files {
	files: Vec<File>,
	cur_file: Option<File>,
}

#[derive(Resource, Debug)]
struct Data {
	duration: u32,
	name: String,
	subtitle: String,
}

#[derive(Resource, Debug)]
struct DataCollection {
	data: Vec<Data>,
}

fn generate_csv(data_collection: &Vec<Data>) {
	let Some(data) = data_collection.last() else {
		return;
	};
	let out = format!("{};{};{}\n", data.duration, data.name, data.subtitle);
	let mut file = std::fs::File::options()
		.append(true)
		.create(true)
		.open("subtitles.csv")
		.unwrap();
	file.write(out.as_bytes()).unwrap();
}

fn generate_subtitle_ai(files: &mut Vec<File>) {
	let tmp_path = std::path::Path::new("tmp");
	if !tmp_path.exists() {
		std::fs::create_dir("tmp").unwrap();
	}
	let mut whisper_cmd = Command::new("gamemoderun");
	whisper_cmd
		.arg("whisper")
		.arg("--model")
		.arg(WHISPER_MODEL)
		.arg("--language")
		.arg(WHISPER_LANGUAGE)
		.arg("--verbose")
		.arg("False")
		.arg("--output_format")
		.arg("txt")
		.arg("--output_dir")
		.arg(tmp_path);

	for file in files.iter() {
		whisper_cmd.arg(file.path.clone());
	}
	whisper_cmd.spawn().unwrap().wait().unwrap();

	for file in files {
		let path = format!(
			"tmp/{}.txt",
			file.path
				.file_name()
				.unwrap()
				.to_string_lossy()
				.strip_suffix(".wav")
				.unwrap()
		);
		let Ok(initial_subtitle) = std::fs::read_to_string(&path) else {
			println!("Failed to open {}", path);
			continue;
		};
		file.initial_subtitle = Some(
			initial_subtitle
				.replace('\n', " ")
				.trim_end()
				.chars()
				.enumerate()
				.map(|(i, c)| {
					if i == 0 {
						c.to_uppercase().next().unwrap()
					} else {
						c
					}
				})
				.collect(),
		);
	}
	std::fs::remove_dir_all(tmp_path).unwrap();
}

fn ui(
	mut ctx: EguiContexts,
	mut files: ResMut<Files>,
	mut data_collection: ResMut<DataCollection>,
	mut complete: ResMut<Complete>,
	mut commands: Commands,
) {
	egui::CentralPanel::default().show(ctx.ctx_mut(), |ui| {
		if complete.complete {
			ui.label("All done");
			return;
		}
		if ui.button("Play next file").clicked() {
			complete.completed += 1;
			generate_csv(&data_collection.data);
			files.cur_file = files.files.pop();
			let Some(cur_file) = &files.cur_file else {
				complete.complete = true;
				return;
			};

			commands.spawn(AudioBundle {
				source: cur_file.handle.clone(),
				settings: PlaybackSettings::ONCE
					.with_volume(bevy::audio::Volume::new_relative(0.5)),
				..default()
			});

			let wav = hound::WavReader::open(cur_file.path.clone()).unwrap();
			let duration = wav.duration() * 1000 / wav.spec().sample_rate;
			let name = cur_file.path.file_name().unwrap().to_string_lossy();
			let name = name.strip_suffix(".hca.wav").unwrap();
			let subtitle = cur_file.initial_subtitle.clone().unwrap_or_default();

			data_collection.data.push(Data {
				duration,
				name: String::from(name),
				subtitle,
			});
		}
		if let Some(cur_file) = &files.cur_file && ui.button("Play audio again").clicked() {
			commands.spawn(AudioBundle {
				source: cur_file.handle.clone(),
				settings: PlaybackSettings::ONCE
					.with_volume(bevy::audio::Volume::new_relative(0.5)),
				..default()
			});
		}
		if let Some(data) = data_collection.data.last_mut() {
			ui.label(format!("Duration: {}ms", data.duration));
			ui.label(format!("Name: {}", data.name));
			ui.add(egui::TextEdit::singleline(&mut data.subtitle).desired_width(f32::INFINITY));
		}
		ui.label(format!(
			"Complete: {}/{}",
			complete.completed, complete.total
		));
	});
}
