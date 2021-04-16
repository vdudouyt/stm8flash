/***************************************************************************
 *   Intel hex read and write utility functions                            *
 *                                                                         *
 *   Copyright (c) Valentin Dudouyt, 2004 2012 - 2014                      *
 *   Copyright (c) Philipp Klaus Krause, 2021                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef IHEX_H
#define IHEX_H

// Read Intel hex file.
// Returns number of bytes read on success, -1 otherwise.
int ihex_read(FILE *pFile, unsigned char *buf, unsigned int start, unsigned int end);

// Write Intel hex file.
// Returns 0 on success, -1 otherwise.
int ihex_write(FILE *pFile, unsigned char *buf, unsigned int start, unsigned int end);

#endif

