#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <omp.h>

#define RND0_1 ((double) random() / ((long long)1<<31))
#define G 6.67408e-11
#define EPSLON 0.0005

typedef struct particle
{
	double x;
	double y;
	double vx;
	double vy;
	double m;
	long ix;
	long jy;

}particle_t;

typedef struct matrix
{
	double mass;
	double cmx;
	double cmy;

}MATRIX;

particle_t *par;
MATRIX **mtr;


/******************************************************************
void init_particles 
long seed:			seed for random generator (input)
long long n_part:	number of particles (input)
particle_t *par:	pointer to particle vector

PURPOSE : 	Initializes particle position, velocity and mass from 
			random value
RETURN :  	void
********************************************************************/
void init_particles(long seed, long ncside, long long n_part, particle_t *par){
	long long i;
    srandom(seed);
    for(i=0; i < n_part; i++)
    {
        par[i].x = RND0_1;
        par[i].y = RND0_1;
        par[i].vx = RND0_1 / ncside / 10.0;
        par[i].vy = RND0_1 / ncside / 10.0;
        par[i].m = RND0_1 * ncside / (G * 1e6 * n_part);
    }
}

/******************************************************************
double centerofmassinit
long ncside:		size of grid(input)
long long n_part:	number of particles (input)
double masssum:		sum of mass of all particles

PURPOSE : 	Calculates first iteration of grid cell center of mass, sum 
			of mass of all particles and which cell particles are in
RETURN :  	masssum
********************************************************************/
double centerofmassinit (long ncside, long long n_part, double masssum){
	#pragma omp parallel for reduction(+:masssum)
	for(long long k=0;k<n_part;k++){
		par[k].ix=par[k].x*ncside;
		par[k].jy=par[k].y*ncside;
		mtr[par[k].ix][par[k].jy].mass+=par[k].m;
		masssum+=par[k].m;
	}
	#pragma omp parallel for
	for(long long k=0; k<n_part; k++){
		mtr[par[k].ix][par[k].jy].cmx+=(par[k].m*par[k].x)/mtr[par[k].ix][par[k].jy].mass; //centre of mass for x, for each grid cell
		mtr[par[k].ix][par[k].jy].cmy+=(par[k].m*par[k].y)/mtr[par[k].ix][par[k].jy].mass; //centre of mass for y, for each grid cell
	}
	return masssum;
}

/******************************************************************
void wrapcalc
long ncside:		size of grid(input)
long long n_part:	number of particles (input)
long particle_iter:	number of iterations to run (input)
double masssum:		sum of mass of all particles

PURPOSE : 	Bulk of calculations for particle position and velocity,
			as well as grid cell center of mass
RETURN :  	void
********************************************************************/
void wrapcalc(long ncside, long long n_part, long particle_iter, double masssum){
	int wwx, wwy;
	int t, u;
	long long k;
	double xcm=0, ycm=0;
	long m[]={0,0,0}, n[]={0,0,0};
	long j;
	double local_vx=0, local_vy=0, local_x=0, local_y=0;
	double rx, ry;
	double compvx=0, compvy=0;

	for(long l=0; l<particle_iter; l++){
		
		for(long i=0; i<ncside; i++){	//set mass in each cell to 0, to be calculated for this iteration
			for(long j=0; j<ncside; j++) mtr[i][j].mass=0;
			}

		#pragma omp parallel
		{
			#pragma omp for private(t,u,compvx, compvy, wwy, wwx, m, n, rx) schedule(guided,4) nowait //calculates position and velocity for each particle in x
			for(k=0; k<n_part; k++){
				compvx=0, compvy=0;
				wwx=0, wwy=0;
				m[1]=par[k].x*ncside;
		       	n[1]=par[k].y*ncside;
				m[2]=m[1]+1,m[0]=m[1]-1,n[2]=n[1]+1,n[0]=n[1]-1;
				if(m[2]>=ncside) {m[2]=0; wwx=1;}
				else if(m[0]<0) {m[0]=ncside-1; wwx=1;}
				if(n[2]>=ncside) {n[2]=0; wwy=1;}
				else if(n[0]<0) {n[0]=ncside-1; wwy=1;}
				for(t=0; t<3; t++){
					for(u=0; u<3; u++){
						rx=mtr[m[t]][n[u]].cmx;
						if(wwx) rx=(-rx);
						if(rx<0 && (-rx)>EPSLON) compvx-=G*mtr[m[t]][n[u]].mass/(rx*rx*9);
						else if(rx>EPSLON) compvx+=G*mtr[m[t]][n[u]].mass/(rx*rx*9);
					}
				}
				par[k].vx+= compvx;
				par[k].x+= par[k].vx + compvx*0.5;
				while(par[k].x>=1) par[k].x-=1;
				while(par[k].x<0) par[k].x+=1;
			}

			#pragma omp for private(t,u, compvy, compvx, wwx, wwy, m, n, ry) schedule(guided,4) nowait //calculates position and velocity for each particle in x
			for(k=0; k<n_part; k++){
				compvx=0, compvy=0;
				wwx=0, wwy=0;
				m[1]=par[k].x*ncside;
	        	n[1]=par[k].y*ncside;
				m[2]=m[1]+1,m[0]=m[1]-1,n[2]=n[1]+1,n[0]=n[1]-1;
				if(m[2]>=ncside) {m[2]=0; wwx=1;}
				else if(m[0]<0) {m[0]=ncside-1; wwx=1;}
				if(n[2]>=ncside) {n[2]=0; wwy=1;}
				else if(n[0]<0) {n[0]=ncside-1; wwy=1;}
				for(t=0; t<3; t++){
					for(u=0; u<3; u++){
						ry=mtr[m[t]][n[u]].cmy;
						if(wwy) ry=(-ry);
						if(ry<0 && (-ry)>EPSLON) compvy-=G*mtr[m[t]][n[u]].mass/(ry*ry*9);
						else if(ry>EPSLON) compvy+=G*mtr[m[t]][n[u]].mass/(ry*ry*9);
					}
				}
				par[k].vy+= compvy;
				par[k].y+= par[k].vy + compvy*0.5;
				while(par[k].y>=1) par[k].y-=1;
				while(par[k].y<0) par[k].y+=1;	
			}
		}
		
		#pragma omp for private(j) collapse(2) schedule(static, 4) //sets centre of mass in each cell to 0, to be calculated for this iteration
		for(long i=0; i<ncside; i++){
			for(j=0; j<ncside; j++){mtr[i][j].cmx=0;mtr[i][j].cmy=0;}
		}
		
		#pragma omp parallel for private(k) reduction(+:xcm) reduction(+:ycm) schedule(dynamic, 4) 
		for(k=0; k<n_part; k++){
			par[k].ix=par[k].x*ncside;
			par[k].jy=par[k].y*ncside;
			mtr[par[k].ix][par[k].jy].mass+=par[k].m;
			mtr[par[k].ix][par[k].jy].cmx+=(par[k].m*par[k].x)/mtr[par[k].ix][par[k].jy].mass; //centre of mass for x, for a given cell
			mtr[par[k].ix][par[k].jy].cmy+=(par[k].m*par[k].y)/mtr[par[k].ix][par[k].jy].mass; //centre of mass for y, for a given cell
			if(l==particle_iter-1){//on the last iteration, calculates centre of mass of all particles
				xcm+=(par[k].m*par[k].x)/masssum;
				ycm+=(par[k].m*par[k].y)/masssum;
			}
		}
	}
	printf("%.2f %.2f\n", par[0].x, par[0].y);
	printf("%.2f %.2f\n", xcm, ycm);
}

/******************************************************************
void usage

PURPOSE : 	To be used in case of input error, prints proper usage
			and exits
RETURN :  	void
********************************************************************/
void usage(){
	printf("Usage: simpar <random generator seed> <grid size> <particle no> <time-step no>\n");
	exit(0);
}

/******************************************************************
void main
int arcg
char** argv

PURPOSE : 	Gets user inputs, allocates memory for matrix and particles,
			and runs calculation functions
RETURN :  	void
********************************************************************/
void main(int argc, char** argv){

	if(argc!=5) usage();
	double masssum=0;
	long l;
	char *ptr1, *ptr2, *ptr3, *ptr4;
	const long seed = strtol(argv[1], &ptr1, 10);
	long ncside = strtol(argv[2], &ptr2, 10);
	long long n_part = strtol(argv[3], &ptr3, 10);
	const long particle_iter = strtol(argv[4], &ptr4, 10);
	if (*ptr1!=0 || *ptr2!=0 || *ptr3!=0 || *ptr4!=0 || seed <=0 || ncside<=0 || n_part<=0 || particle_iter<=0) usage();

	if ((par = (particle_t*)calloc(n_part,sizeof(particle_t)))==NULL) exit (0);

	if ((mtr = (MATRIX**)calloc(ncside,sizeof(MATRIX*)))==NULL) exit (0);

	for (l=0; l<ncside; l++){
		if ((mtr[l]=(MATRIX*)calloc(ncside,sizeof(MATRIX)))==NULL) exit (0);
	}
	
	init_particles(seed, ncside, n_part, par);
	masssum=centerofmassinit(ncside, n_part, masssum);
	wrapcalc(ncside, n_part, particle_iter, masssum);

}

