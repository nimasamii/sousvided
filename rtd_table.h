#ifndef SOUSVIDED_RTD_TABLE_H
#define SOUSVIDED_RTD_TABLE_H

extern float *RTD_TABLE;

int init_rtd_table(const double T0, const double Tmax, const double dT,
		   const double R0, const double r_ref);

void free_rtd_table();

#endif /* SOUSVIDED_RTD_TABLE_H */
