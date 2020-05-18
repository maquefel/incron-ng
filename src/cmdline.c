// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
/*
Copyright (c) 2019, Nikita Shubin <me@maquefel.me>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the "cmdline" project.
*/

/*
* Parse cmdline into argc, argv[]
*/

#include "cmdline.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>


static const char _cmdline_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1,
	3, 1, 4, 1, 5, 1, 6, 1,
	7, 1, 8, 1, 11, 2, 2, 9,
	2, 2, 10, 2, 2, 11, 2, 3,
	2, 3, 3, 2, 9, 3, 3, 2,
	10, 3, 3, 2, 11, 0
};

static const char _cmdline_key_offsets[] = {
	0, 0, 0, 2, 4, 6, 8, 13,
	18, 23, 23, 23, 0
};

static const char _cmdline_trans_keys[] = {
	34, 92, 34, 92, 39, 92, 39, 92,
	9, 32, 34, 39, 92, 9, 32, 34,
	39, 92, 9, 32, 34, 39, 92, 0
};

static const char _cmdline_single_lengths[] = {
	0, 0, 2, 2, 2, 2, 5, 5,
	5, 0, 0, 0, 0
};

static const char _cmdline_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0
};

static const char _cmdline_index_offsets[] = {
	0, 0, 1, 4, 7, 10, 13, 19,
	25, 31, 32, 33, 0
};

static const char _cmdline_trans_cond_spaces[] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, 0
};

static const char _cmdline_trans_offsets[] = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 0
};

static const char _cmdline_trans_lengths[] = {
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 0
};

static const char _cmdline_cond_keys[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0
};

static const char _cmdline_cond_targs[] = {
	9, 0, 3, 3, 10, 3, 3, 0,
	5, 5, 11, 5, 5, 6, 6, 8,
	8, 7, 7, 6, 6, 8, 8, 7,
	7, 6, 6, 8, 8, 7, 7, 0,
	0, 0, 0
};

static const char _cmdline_cond_actions[] = {
	9, 0, 11, 0, 13, 11, 0, 0,
	15, 0, 17, 15, 0, 0, 0, 24,
	21, 27, 5, 7, 7, 37, 33, 19,
	0, 7, 7, 37, 33, 41, 30, 0,
	0, 0, 0
};

static const char _cmdline_eof_actions[] = {
	0, 0, 0, 1, 0, 3, 0, 7,
	7, 0, 0, 0, 0
};

static const char _cmdline_nfa_targs[] = {
	0, 0
};

static const char _cmdline_nfa_offsets[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0
};

static const char _cmdline_nfa_push_actions[] = {
	0, 0
};

static const char _cmdline_nfa_pop_trans[] = {
	0, 0
};

static const int cmdline_start = 6;
static const int cmdline_first_final = 6;
static const int cmdline_error = 0;

static const int cmdline_en_skip = 1;
static const int cmdline_en_dquote = 2;
static const int cmdline_en_squote = 4;
static const int cmdline_en_main = 6;



int buildargv(char* input, char*** argv, int *argc)
{
	const char *p = input;
	const char *pe = input + strlen(input);
	const char *eof = pe;
	const char* ts, * te;
	int cs, act, top, stack[2], curline;
	
	const char *argv_s;
	argv_s = p;
	
	int ret=0;
	int errsv=0;
	
	(*argv) = 0;
	char **nargv;
	int argc_=0;
	
	
	{
		cs = (int)cmdline_start;
		top = 0;
	}
	
	{
		int _klen;
		unsigned int _trans = 0;
		unsigned int _cond = 0;
		const char *_acts;
		unsigned int _nacts;
		const char *_keys;
		const char *_ckeys;
		int _cpc;
		{
			
			if ( p == pe )
			goto _test_eof;
			if ( cs == 0 )
			goto _out;
			_resume:  {
				_keys = ( _cmdline_trans_keys + (_cmdline_key_offsets[cs]));
				_trans = (unsigned int)_cmdline_index_offsets[cs];
				
				_klen = (int)_cmdline_single_lengths[cs];
				if ( _klen > 0 ) {
					const char *_lower;
					const char *_mid;
					const char *_upper;
					_lower = _keys;
					_upper = _keys + _klen - 1;
					while ( 1 ) {
						if ( _upper < _lower )
						break;
						
						_mid = _lower + ((_upper-_lower) >> 1);
						if ( ( (*( p))) < (*( _mid)) )
						_upper = _mid - 1;
						else if ( ( (*( p))) > (*( _mid)) )
						_lower = _mid + 1;
						else {
							_trans += (unsigned int)(_mid - _keys);
							goto _match;
						}
					}
					_keys += _klen;
					_trans += (unsigned int)_klen;
				}
				
				_klen = (int)_cmdline_range_lengths[cs];
				if ( _klen > 0 ) {
					const char *_lower;
					const char *_mid;
					const char *_upper;
					_lower = _keys;
					_upper = _keys + (_klen<<1) - 2;
					while ( 1 ) {
						if ( _upper < _lower )
						break;
						
						_mid = _lower + (((_upper-_lower) >> 1) & ~1);
						if ( ( (*( p))) < (*( _mid)) )
						_upper = _mid - 2;
						else if ( ( (*( p))) > (*( _mid + 1)) )
						_lower = _mid + 2;
						else {
							_trans += (unsigned int)((_mid - _keys)>>1);
							goto _match;
						}
					}
					_trans += (unsigned int)_klen;
				}
				
			}
			_match:  {
				_ckeys = ( _cmdline_cond_keys + (_cmdline_trans_offsets[_trans]));
				_klen = (int)_cmdline_trans_lengths[_trans];
				_cond = (unsigned int)_cmdline_trans_offsets[_trans];
				
				_cpc = 0;
				{
					const char *_lower;
					const char *_mid;
					const char *_upper;
					_lower = _ckeys;
					_upper = _ckeys + _klen - 1;
					while ( 1 ) {
						if ( _upper < _lower )
						break;
						
						_mid = _lower + ((_upper-_lower) >> 1);
						if ( _cpc < (int)(*( _mid)) )
						_upper = _mid - 1;
						else if ( _cpc > (int)(*( _mid)) )
						_lower = _mid + 1;
						else {
							_cond += (unsigned int)(_mid - _ckeys);
							goto _match_cond;
						}
					}
					cs = 0;
					goto _again;
				}
			}
			_match_cond:  {
				cs = (int)_cmdline_cond_targs[_cond];
				
				if ( _cmdline_cond_actions[_cond] == 0 )
				goto _again;
				
				_acts = ( _cmdline_actions + (_cmdline_cond_actions[_cond]));
				_nacts = (unsigned int)(*( _acts));
				_acts += 1;
				while ( _nacts > 0 )
				{
					switch ( (*( _acts)) )
					{
						case 2:  {
							{
								
								argv_s = p;
							}
							break; }
						case 3:  {
							{
								
								nargv = (char**)realloc((*argv), (argc_ + 1)*sizeof(char*));
								(*argv) = nargv;
								(*argv)[argc_] = strndup(argv_s, p - argv_s);
								argc_++;
							}
							break; }
						case 4:  {
							{
								{top-= 1;cs = stack[top]; goto _again;} }
							break; }
						case 5:  {
							{
								{stack[top] = cs; top += 1;cs = 1; goto _again;}}
							break; }
						case 6:  {
							{
								{top-= 1;cs = stack[top]; goto _again;} }
							break; }
						case 7:  {
							{
								{stack[top] = cs; top += 1;cs = 1; goto _again;}}
							break; }
						case 8:  {
							{
								{top-= 1;cs = stack[top]; goto _again;} }
							break; }
						case 9:  {
							{
								{stack[top] = cs; top += 1;cs = 4; goto _again;}}
							break; }
						case 10:  {
							{
								{stack[top] = cs; top += 1;cs = 2; goto _again;}}
							break; }
						case 11:  {
							{
								{stack[top] = cs; top += 1;cs = 1; goto _again;}}
							break; }
					}
					_nacts -= 1;
					_acts += 1;
				}
				
				
			}
			_again:  {
				if ( cs == 0 )
				goto _out;
				p += 1;
				if ( p != pe )
				goto _resume;
			}
			_test_eof:  { {}
				if ( p == eof )
				{
					const char *__acts;
					unsigned int __nacts;
					__acts = ( _cmdline_actions + (_cmdline_eof_actions[cs]));
					__nacts = (unsigned int)(*( __acts));
					__acts += 1;
					while ( __nacts > 0 ) {
						switch ( (*( __acts)) ) {
							case 0:  {
								{
									
									ret = -1;
									errsv = BUILDARGV_EDQUOTE;
								}
								break; }
							case 1:  {
								{
									
									ret = -1;
									errsv = BUILDARGV_ESQUOTE;
								}
								break; }
							case 3:  {
								{
									
									nargv = (char**)realloc((*argv), (argc_ + 1)*sizeof(char*));
									(*argv) = nargv;
									(*argv)[argc_] = strndup(argv_s, p - argv_s);
									argc_++;
								}
								break; }
						}
						__nacts -= 1;
						__acts += 1;
					}
				}
				
			}
			_out:  { {}
			}
		}
	}
	
	
	/** last element zero */
	nargv = (char**)realloc((*argv), (argc_ + 1)*sizeof(char*));
	(*argv) = nargv;
	(*argv)[argc_] = 0;
	
	*argc = argc_;
	
	if(ret == -1)
	{
		errno = errsv;
		return -1;
	}
	
	return 0;
};
