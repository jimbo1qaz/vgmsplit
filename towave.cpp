//towave - a program to extract the channels from a chiptune file
//Copyright 2011 Bryan Mitchell

// towave-j fork by nyanpasu64, 2018-

/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "CLI11/CLI11.hpp"

#include "wave_writer.h"
#include "gme/gme.h"

#include <iostream>
#include <string>
#include <sstream>


using gme_t = Music_Emu;
using std::string;

static const char* APP_NAME = "towave-j";
static const int BUF_SIZE = 1024;

static const int MS_SECOND = 1000;


std::string num2str(int x) {
	std::stringstream result;
	
	result << x;
	
	return result.str();
}

void writeTheWave(gme_t *emu, int tracklen_ms, int i, int sample_rate) {
	//Create a muting mask to isolate the channel
	int mute = -1;
	mute ^= (1 << i);
	gme_mute_voices(emu, mute);
	
	//Create a buffer to hand the data from GME to wave_write
	short buffer[BUF_SIZE];
	
	//The filename will be a number, followed by a space and its track title.
	//This ensures both unique and (in most cases) descriptive file names.
	std::string wav_name = num2str(i+1);
	wav_name += "-";
	wav_name += (std::string)gme_voice_name(emu, i);
	wav_name += ".wav";
	
	//Sets up the header of the WAV file so it is, in fact, a WAV
	wave_open(sample_rate, wav_name.c_str());
	wave_enable_stereo(); //GME always outputs in stereo
	
	//Perform the magic.
	while (gme_tell(emu) < tracklen_ms) {
		//If an error occurs during play, we still need to close out the file
		if (gme_play(emu, BUF_SIZE, buffer)) break;
		wave_write(buffer, BUF_SIZE);
	}
	
	//Properly finishes the header and closes the internal file object
	wave_close();
}

int main ( int argc, char** argv ) {
	CLI::App app{"towave-j: Program to record channels from chiptune files"};

	std::string filename;
	int tracknum = 1;
	int tracklen_ms;
	{
		double _tracklen_s = -1;

		app.add_option("filename", filename, "Any music file accepted by GME")->required();
		app.add_option("tracknum", tracknum, "Track number (first track is 1)");
		app.add_option("tracklen", _tracklen_s, "How long to record, in seconds");
		app.failure_message(CLI::FailureMessage::help);
		CLI11_PARSE(app, argc, argv);

		tracknum -= 1;
		tracklen_ms = static_cast<int>(_tracklen_s * 1000);
	}

	gme_t* emu;
	int sample_rate = 44100;
	const char* err1 = gme_open_file(filename.c_str(), &emu, sample_rate);
	
	if (err1) {
		std::cout << err1;
		return 1;
	}

	//long periods of silence. Unfortunately, this also means that
	//if a track is of finite length, we still need to have its length separately.
	gme_ignore_silence(emu, true);
	const char* err2 = gme_start_track(emu, tracknum);
	if (err2) {
		std::cout << err2;
		return 1;
	}
	//Run the emulator for a second while muted to eliminate opening sound glitch
	for (int len = 0; len < MS_SECOND; len = gme_tell(emu)) {
		int m = -1;
		m ^= 1;
		gme_mute_voices(emu, m);
		short buf[BUF_SIZE];
		gme_play(emu, BUF_SIZE, buf);
	}

	// If no length specified, obtain track length from file.
	if (tracklen_ms < 0) {
		gme_info_t* info;
		gme_track_info(emu, &info, tracknum);

		tracklen_ms = info->play_length;  // Guaranteed to be either valid, or 2.5 minutes.
		delete info;
	}
	
	for (int i = 0; i < gme_voice_count(emu); i++) {
		const char* err = gme_start_track(emu, tracknum);
		if (err) {
			std::cout << err;
			return 1;
		}
		writeTheWave(emu, tracklen_ms, i, sample_rate);
	}
	
	gme_delete(emu);
	
	return 0;
}
