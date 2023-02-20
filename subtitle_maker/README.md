## This tool for making subtitles is _very_ jank

### Usage

1. Extract SOUND/PC/TIKYUU5_VOICE_LIST.LANG.AWE (or equivalent file)
2. Convert all hca files to wavs (See #Converting HCAs to batches)
3. Move into batches (I did batches of 100 but anywhere up to 500 is probably fine)
4. Install <https://github.com/openai/whisper>
5. Change the batch path and whisper options in src/main.rs
6. Compile and run the application (See #Building)
7. Click on "Play next file" then subtitle the played audio until done with the batch
8. Repeat with all batches until all audio files are subtitled

### Building

This project is built with rust which can be installed from <https://rustup.rs/>

This makes use of egui through bevy_egui and as such needs some dependencies installed on linux systems, however not for windows or macos

On debian based systems these can be installed with

```
sudo apt install libxcb-render0-dev libxcb-shape0-dev libxcb-xfixes0-dev
```

### Converting HCAs to batches

```nu
# Convert each
ls | each { |it| vgmstream-cli $it.name }
# Delete old hcas
ls | where name !~ ".wav" | each { |file| rm -f $file.name }
# Delete duplicate files
ls | where name =~ '_.\.hca\.wav$' | each { |file| rm $file.name }
# Create batches of size 100
ls | each -n { |it| let index = ($it.index / 100 + 1 | into int); mkdir $"batch ($index)"; mv $it.item.name $"batch ($index)" }
```
