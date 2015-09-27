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
#include <vector>
#include <list>
#include <map>
using namespace std;

//#define USE_OLD_ENGINE

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
							throw(0);
#define readUSHORT(var)	fin.read(reinterpret_cast<char*>(&(var)), 2);                    \
						if (fin.eof())                                                   \
							throw(0);
#define readBYTE(var)	fin.read(reinterpret_cast<char*>(&(var)), 1);                    \
						if (fin.eof())                                                   \
							throw(0);

UINT gcd(UINT a, UINT b)
{
	UINT t;
	while (b != 0)
	{
		t = a % b;
		a = b;
		b = t;
	}
	return a;
}

//6~77
USHORT offset = 24;
const char input_file[] = "tmp.mid";
const char output_file[] = "output";

typedef std::list<USHORT> note_table_c;
typedef std::map<UINT, note_table_c> note_table;
typedef std::map<UINT, UINT> tempo_table;
note_table notes;
tempo_table tempo;

const double least_interval = 0.05;
const double interval_unit = 0.025;

int main()
{
	std::ifstream fin(input_file, std::ios_base::in | std::ios_base::binary);
	std::ofstream fout(output_file);

	USHORT note_unit;
	UINT note_count = 0;
	USHORT highest = 0, lowest = USHRT_MAX;
	try
	{
		UINT cur_track, head;
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

		for (cur_track = 0; cur_track < nn; cur_track++)
		{
			UINT tick = 0;
			readUINT(head);
			if (head != 0x6B72544D)
				throw(0);
			UINT track_byte_t;
			readUINT(track_byte_t);
			track_byte_t = UINT32_SWAP(track_byte_t);
			long long track_byte = track_byte_t;

			while (track_byte > 0)
			{
				UINT nextTick = 0;
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

								switch (event)
								{
									case 0x51:
									{
										std::vector<BYTE> tmp(len);
										fin.read(reinterpret_cast<char*>(tmp.data()), len);
										if (fin.eof())
											throw(0);

										UINT new_tempo = 0;
										for (size_t i = 0; i < len; i++)
											new_tempo = (new_tempo << 8) | tmp[i];
										tempo.emplace(tick, new_tempo);

										break;
									}
									case 0x2F:
										track_byte = 0;
									default:
										if (track_byte < len)
											break;
										skip(len);
										track_byte -= len;
								}
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
	tempo.emplace(ULONG_MAX, 0);

	cout << offset << ':' << lowest - offset << '~' << highest - offset << endl;
	cin >> offset;
	cout << offset << ':' << lowest - offset << '~' << highest - offset << endl;
	fout << note_count << endl;
	note_table::iterator itr = notes.begin(), itrEnd = notes.end(), itrSeg, itrSegEnd;
#ifndef USE_OLD_ENGINE
	UINT last_tick = itr->first;
	double last_interval = 0.1;
	tempo_table::iterator itrT = tempo.begin(), itrTEnd = tempo.end();

	UINT cur_tempo = itrT->second;
	itrT++;

	UINT delta = 0;

	for (; itrT != itrTEnd; itrT++)
	{
		UINT last_tick_bak = last_tick;
		UINT note_unit_seg = 0;
		itrSeg = itr;
		while (true)
		{
			if (itr == itrEnd)
			{
				itrSegEnd = itrEnd;
				break;
			}
			UINT this_tick = itr->first;
			if (this_tick >= itrT->first)
			{
				itrSegEnd = itr;
				break;
			}
			UINT delta = this_tick - last_tick_bak;
			if (delta != 0)
			{
				if (note_unit_seg == 0)
					note_unit_seg = delta;
				else
					note_unit_seg = gcd(note_unit_seg, delta);
				last_tick_bak = this_tick;
			}
			itr++;
		}
		if (note_unit_seg == 0)
			continue;

		double this_interval = (static_cast<double>(note_unit_seg) / note_unit) * cur_tempo / 1000000;
		if (this_interval < least_interval)
		{
			this_interval = least_interval;
			long long target_note_unit_seg = static_cast<long long>(this_interval * 1000000 / cur_tempo * note_unit);

			double note_unit_seg_div_2 = note_unit, next_note_unit_seg_div = note_unit_seg_div_2 / 2;
			while (abs(next_note_unit_seg_div - target_note_unit_seg) < abs(note_unit_seg_div_2 - target_note_unit_seg))
			{
				note_unit_seg_div_2 = next_note_unit_seg_div;
				next_note_unit_seg_div = note_unit_seg_div_2 / 2;
			}

			note_unit_seg = static_cast<UINT>(note_unit_seg_div_2);
		}
		else
			this_interval = static_cast<long long>(this_interval / interval_unit + 0.5) * interval_unit;
		if (this_interval != last_interval)
		{
			fout << "-2" << endl << this_interval << endl;
			last_interval = this_interval;
		}
		cur_tempo = itrT->second;

		for (; itrSeg != itrSegEnd; itrSeg++)
		{
			UINT this_tick = itrSeg->first;
			for (delta += this_tick - last_tick; delta >= note_unit_seg; delta -= note_unit_seg)
				fout << "-1" << endl;
			last_tick = this_tick;
			std::for_each(itrSeg->second.begin(), itrSeg->second.end(), [&fout](int note) {
				note -= offset;
				if (note >= 0)
					fout << note << endl;
			});
		}
		UINT this_tick = itrT->first;
		if (this_tick == ULONG_MAX)
			continue;
		for (delta += this_tick - last_tick; delta >= note_unit_seg; delta -= note_unit_seg)
			fout << "-1" << endl;
		last_tick = this_tick;
	}
#else
	note_unit /= 4;
	UINT last_tick = itr->first / note_unit;
	double delta = 0;
	for (; itr != itrEnd; itr++)
	{
		UINT this_tick = itr->first / note_unit;
		for (delta += (this_tick - last_tick); delta > 1; delta -= 1)
			fout << "-1" << endl;
		last_tick = this_tick;
		std::for_each(itr->second.begin(), itr->second.end(), [&fout](USHORT note) {
			note -= offset;
			fout << note << endl;
		});
	}
#endif

	fin.close();
	fout.close();
	system("pause");
	return 0;
}
