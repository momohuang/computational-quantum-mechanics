#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <utility>
#include <queue>
#include <vector>
#include <random>
using namespace std;

const double pi = M_PI;
const double hbar = 1.0;
default_random_engine generator;
normal_distribution<double> Gauss(0.0, 1.0);
uniform_real_distribution<double> Unif(0.0,1.0);

// Physical system consists of N particles in d-dimension world
typedef vector<vector<double> > phys_sys;
const int N = 2; // Number of particles in the world
const int d = 3; // d dimension world
double mass[N] = {1, 1}; // mass of each particles
const int Nu = 2; // Nucleus number
double nucl_pos[Nu][3] = {{0, 0, -1.398/2}, {0, 0, 1.398/2}}; // Nucleus position
double potential(phys_sys &R){
	double V = 0;

	/*
	// SHO
	for(int i = 0; i < N; i++){
		for(int j = 0; j < d; j++)
			V += 0.5 * (R[i][j]) * (R[i][j]);
	}
	*/

	/*
	// Bump (Wizard Hat)
	for(int i = 0; i < N; i++){
		double r = 0;
		for(int j = 0; j < d; j++)
			r += (R[i][j]) * (R[i][j]);
		r = sqrt(r);

		V += 0.5 * (r - 3) * (r - 3);
	}
	*/

	// Nucleus
	for(int n = 0; n < Nu; n++){
		for(int i = 0; i < N; i++){
			double r2 = 0;
			for(int j = 0; j < d; j++)
				r2 += (R[i][j] - nucl_pos[n][j]) * (R[i][j] - nucl_pos[n][j]);
			V += -1.0 / sqrt(r2);
		}
	}

	// Repulsive Coulomb Potential
	for(int i1 = 0; i1 < N; i1++){
		for(int i2 = i1 + 1; i2 < N; i2++){
			double r2 = 0;
			for(int j = 0; j < d; j++)
				r2 += (R[i1][j]-R[i2][j]) * (R[i1][j]-R[i2][j]);
			V += 1.0 / sqrt(r2);
		}
	}

	return V;
}

// Parameters for Quantum-Monte-Carlo
struct QMC_system{
	vector<phys_sys> walkers;
	double E_guess;
};
const int N0_walker = 5000; // Number of initial random-walkers
double E0_guess = 0; // initial guess of ground state energy
int thermal_steps = 400; // # of time steps for random walking
int stable_steps = 400; // # of time steps for calculating properties
double dt = 0.15; // time slice in Quantum-Monte-Carlo
double alpha = 1.0; // adaptive rate for birth-death process
FILE *num_walker = fopen("walkerN.txt", "w");
FILE *qmc_system = fopen("QMC_system.txt", "w");

void initialization(QMC_system &QMC){
	QMC.E_guess = E0_guess;

	for(int p = 0; p < N0_walker; p++){
		QMC.walkers.push_back(phys_sys());

		phys_sys &R = QMC.walkers[p];
		for(int i = 0; i < N; i++){
			R.push_back(vector<double>());

			for(int j = 0; j < d; j++)
				R[i].push_back(Gauss(generator));
		}
	}
}

phys_sys random_walk(phys_sys &R){
	phys_sys R_next = R;
	for(int i = 0; i < N; i++){
		for(int j = 0; j < d; j++){
			R_next[i][j] += sqrt(hbar * dt / mass[i]) * Gauss(generator);
		}
	}
	return R_next;
}

void export_QMC_system(QMC_system &QMC){
	int N_walker = (int)QMC.walkers.size();
	fprintf(num_walker, "%d\n", N_walker);

	for(int p = 0; p < N_walker; p++){
		for(int i = 0; i < N; i++){
			for(int j = 0; j < d; j++){
				fprintf(qmc_system, "%f ", QMC.walkers[p][i][j]);
			}
		}
		fprintf(qmc_system, "\n");
	}
}

QMC_system propagate(QMC_system &QMC){
	int N_walker = (int)QMC.walkers.size();
	//printf("%d %f\n", N_walker, QMC.E_guess);
	//if(N_walker == 0) while(1);

	double V_mean = 0;
	QMC_system QMC_next;

	for(int p = 0; p < N_walker; p++){
		// Gaussian Random Walk
		phys_sys R = random_walk(QMC.walkers[p]);

		// Birth-Death Process
		double V = potential(R);
		int replicas = min((int)(min(exp(-dt / hbar * (V - QMC.E_guess)), 10.0) + Unif(generator)), 3);

		for(int i = 0; i < replicas; i++)
			QMC_next.walkers.push_back(R);

		V_mean += V * replicas;
	}

	int N_walker_next = (int)QMC_next.walkers.size();
	V_mean /= N_walker_next;

	QMC_next.E_guess = V_mean + alpha * (1.0 - 1.0 * N_walker_next / N0_walker);

	return QMC_next;
}

int main(int argc, char* argv[]){
	QMC_system QMC;
	initialization(QMC);

	for(int t = 0; t < thermal_steps; t++)
		QMC = propagate(QMC);

	vector<double> E_est;
	for(int t = 0; t < stable_steps; t++){
		export_QMC_system(QMC);
		QMC = propagate(QMC);
		E_est.push_back(QMC.E_guess);
	}

	// Calculating Mean and Std. of Ground State Energy
	double E_mean = 0;
	for(int t = 0; t < stable_steps; t++)
		E_mean += E_est[t] / stable_steps;
	double E_var = 0;
	for(int t = 0; t < stable_steps / 25; t++){
		double E_local_mean = 0;
		for(int t2 = 0; t2 < 25; t2 ++)
			E_local_mean += E_est[t * 25 + t2] / 25.0;
		E_var += (E_local_mean - E_mean) * (E_local_mean - E_mean) / (stable_steps / 25 - 1);
	}
	double E_std = sqrt(E_var);

	printf("Ground state energy: %f +/- %f\n", (E_mean + 1.0/1.398) * 27.2, E_std * 27.2);
}
