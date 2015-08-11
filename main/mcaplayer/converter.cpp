/*
A midi converter for my ComputerCraft program:mcaplay
Copyright (C) <2015>  <xy12423>

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
#include <iostream>
#include <fstream>
#include <algorithm>
#include <list>
#include <map>
using namespace std;

typedef unsigned short int USHORT;
typedef unsigned int UINT;
typedef unsigned long ULONG;
typedef unsigned long long ULONGLONG;
typedef unsigned char BYTE;

#define UINT16_SWAP(val) \
   ((USHORT) ( \
    (((USHORT) (val) & (USHORT) 0x00ffU) << 8) | \
    (((USHORT) (val) & (USHORT) 0xff00U) >> 8)))
#define UINT32_SWAP(val) \
   ((UINT) ( \
    (((UINT) (val) & (UINT) 0x000000ffU) << 24) | \
    (((UINT) (val) & (UINT) 0x0000ff00U) <<  8) | \
    (((UINT) (val) & (UINT) 0x00ff0000U) >>  8) | \
    (((UINT) (val) & (UINT) 0xff000000U) >> 24)))

#define skip(n)	fin.seekg(n, std::ios_base::cur);
#define readUINT(var)	fin.read(reinterpret_cast<char*>(&(var)), 4);                    \
						if (fin.eof())                                                   \
							return 0;
#define readUSHORT(var)	fin.read(reinterpret_cast<char*>(&(var)), 2);                    \
						if (fin.eof())                                                   \
							return 0;
#define readBYTE(var)	fin.read(reinterpret_cast<char*>(&(var)), 1);                    \
						if (fin.eof())                                                   \
							return 0;

const USHORT offset = 24;
const char input_file[] = "tmp.mid";
const char output_file[] = "output";

typedef std::list<USHORT> note_table_c;
typedef std::map<ULONGLONG, note_table_c> note_table;
note_table notes;

int main()
{
	std::ifstream fin(input_file, std::ios_base::in | std::ios_base::binary);
	std::ofstream fout(output_file);
	
	USHORT note_unit;
	ULONGLONG note_count = 0;
	USHORT highest = 0, lowest = USHRT_MAX;
	try
	{
		UINT i, head;
		readUINT(head);
		if (head != 0x6468544D)
			throw(0);
		skip(4);
		USHORT ff, nn;
		readUSHORT(ff);
		ff = UINT16_SWAP(ff);
		readUSHORT(nn);
		nn = UINT16_SWAP(nn);
		readUSHORT(note_unit);
		note_unit = UINT16_SWAP(note_unit);

		for (i = 0; i < nn; i++)
		{
			ULONGLONG tick = 0;
			readUINT(head);
			if (head != 0x6B72544D)
				throw(0);
			UINT track_byte_t;
			readUINT(track_byte_t);
			track_byte_t = UINT32_SWAP(track_byte_t);
			long long track_byte = track_byte_t;

			while (track_byte > 0)
			{
				ULONGLONG nextTick = 0;
				while (true)
				{
					BYTE sect;
					if (track_byte < 1)
						break;
					readBYTE(sect);
					track_byte--;
					nextTick = (nextTick << 7) | (sect & 0x7F);
					if (!(sect & 0x80))
						break;
				}
				tick += nextTick;

				BYTE event;
				if (track_byte < 1)
					break;
				readBYTE(event);
				track_byte--;
				if (event & 0x80)
				{
					switch (event >> 4)
					{
						case 0x9:
							if (track_byte < 1)
								break;
							readBYTE(event);
							track_byte--;
							notes[tick].push_back(event);
							if (event > highest)
								highest = event;
							if (event < lowest)
								lowest = event;
							note_count++;
							if (track_byte < 1)
								break;
							skip(1);
							track_byte--;
							break;
						case 0x8:
						case 0xA:
						case 0xB:
						case 0xE:
							if (track_byte < 2)
								break;
							skip(2);
							track_byte -= 2;
							break;
						case 0xC:
						case 0xD:
							if (track_byte < 1)
								break;
							skip(1);
							track_byte--;
							break;
						case 0xF:
							if (event == 0xF0 || event == 0xF7)
							{
								while (true)
								{
									BYTE sect;
									if (track_byte < 1)
										break;
									readBYTE(sect);
									track_byte--;
									if (sect == 0xF7)
										break;
								}
							}
							else if (event == 0xFF)
							{
								if (track_byte < 1)
									break;
								readBYTE(event);
								track_byte--;
								BYTE len;
								if (track_byte < 1)
									break;
								readBYTE(len);
								track_byte--;
								if (track_byte < len)
									break;
								skip(len);
								track_byte -= len;
								
								if (event == 0x2F)
									track_byte = 0;
							}
							else
								throw(0);
							break;
					}
				}
				else
				{
					if (track_byte < 1)
						break;
					BYTE velocity;
					readBYTE(velocity);
					track_byte--;
					if (velocity != 0)
					{
						notes[tick].push_back(event);
						if (event > highest)
							highest = event;
						if (event < lowest)
							lowest = event;
						note_count++;
					}
				}
			}
		}
	}
	catch(...){}

	cout << lowest - offset << '~' << highest - offset << endl;
	fout << note_count << endl;
	note_unit /= 4;
	note_table::iterator itr = notes.begin(), itrEnd = notes.end();
	ULONGLONG last_tick = itr->first / note_unit;
	for (; itr != itrEnd; itr++)
	{
		ULONGLONG this_tick = itr->first / note_unit;
		for (ULONGLONG delta = (this_tick - last_tick); delta != 0; delta--)
			fout << "-1" << endl;
		last_tick = this_tick;
		std::for_each(itr->second.begin(), itr->second.end(), [&fout](USHORT note){
			note -= offset;
			fout << note << endl;
		});
	}

	fin.close();
	fout.close();
#ifdef WIN32
	system("pause");
#endif
	return 0;
}
