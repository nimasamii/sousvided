/*
sousvided the Sous Vide deamon for the RaspberryPi
Copyright (C) 2015  Nima Saed-Samii

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef SOUSVIDED_RTD_TABLE_H
#define SOUSVIDED_RTD_TABLE_H

extern float *RTD_TABLE;

int init_rtd_table(const double T0, const double Tmax, const double dT,
		   const double R0, const double r_ref);

void free_rtd_table();

#endif /* SOUSVIDED_RTD_TABLE_H */
