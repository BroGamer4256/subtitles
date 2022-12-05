#![feature(let_chains)]
use bevy::prelude::*;
use bevy_egui::*;
use std::cmp::Ordering::*;
use std::io::Write;
use std::path::PathBuf;

fn main() {
	App::new()
		.add_plugins(DefaultPlugins)
		.add_plugin(EguiPlugin)
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
		.add_startup_system(load_files)
		.add_system(ui)
		.run();
}

fn load_files(
	mut files_res: ResMut<Files>,
	mut complete: ResMut<Complete>,
	asset_server: ResMut<AssetServer>,
) {
	let mut files = vec![];
	for file in std::fs::read_dir(
		"/Path/To/Batch",
	)
	.unwrap()
	{
		let path = file.unwrap().path();
		let handle = asset_server.load(path.clone());
		files.push(File {
			path,
			handle,
		});
	}
	files.sort_by(|a, b| {
		let a = a.path.file_name().unwrap().to_string_lossy();
		let a = a.strip_suffix(".hca.wav").unwrap();
		let b = b.path.file_name().unwrap().to_string_lossy();
		let b = b.strip_suffix(".hca.wav").unwrap();
		let a_e = a.ends_with("_E");
		let b_e = b.ends_with("_E");
		if a_e && b_e {
			a.cmp(b)
		} else if a_e && !b_e {
			Greater
		} else if !a_e && b_e {
			Less
		} else {
			a.cmp(b)
		}
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
	let mut out = String::new();
	for data in data_collection {
		out.push_str(&format!(
			"{},{},{}\n",
			data.duration, data.name, data.subtitle
		));
	}
	let mut file = std::fs::File::create("progress.csv").unwrap();
	file.write_all(out.as_bytes()).unwrap();
}

fn ui(
	mut ctx: ResMut<EguiContext>,
	mut files: ResMut<Files>,
	mut data_collection: ResMut<DataCollection>,
	mut complete: ResMut<Complete>,
	audio: Res<Audio>,
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
			let cur_file = if let Some(file) = &files.cur_file {
				file
			} else {
				complete.complete = true;
				return;
			};

			audio.play_with_settings(
				cur_file.handle.clone(),
				PlaybackSettings::ONCE.with_volume(0.5),
			);

			let wav = hound::WavReader::open(cur_file.path.clone()).unwrap();
			let duration = wav.duration() * 1000 / wav.spec().sample_rate;
			let name = cur_file.path.file_name().unwrap().to_string_lossy();
			let name = name.strip_suffix(".hca.wav").unwrap();
			let mut subtitle = String::new();
			if name.ends_with("_E") &&  let Some(data) = data_collection.data.iter()
					.find(|data| data.name == name.strip_suffix("_E").unwrap())
				{
					subtitle = data.subtitle.clone();
				}

			data_collection.data.push(Data {
				duration,
				name: String::from(name),
				subtitle,
			});
		}
		if let Some(cur_file) = &files.cur_file && ui.button("Play audio again").clicked() {
			audio.play_with_settings(
				cur_file.handle.clone(),
				PlaybackSettings::ONCE.with_volume(0.5),
			);
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
