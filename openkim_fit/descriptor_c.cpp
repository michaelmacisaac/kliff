#include "descriptor_c.h"
#include <iostream>

#define DIM 3
typedef double VectorOfSizeDIM[3];


Descriptor::Descriptor(){
    has_three_body_ = false;
  }

Descriptor::~Descriptor() {
	for (size_t i=0; i<params_.size(); i++) {
		Deallocate2DArray(params_.at(i));
	}
}

void Descriptor::set_cutoff(char* name, int Nspecies, double* rcuts)
{
	if (strcmp(name, "cos") == 0) {
		cutoff_ = &cut_cos;
		d_cutoff_ = &d_cut_cos;
	}
	else if (strcmp(name, "exp") == 0) {
		cutoff_ = &cut_exp;
		d_cutoff_ = &d_cut_exp;
	}

  // store number of species and cutoff values
  Nspecies_ = Nspecies;
  AllocateAndInitialize2DArray(rcuts_, Nspecies, Nspecies);
  int idx = 0;
  for (int i=0; i<Nspecies; i++) {
    for (int j=0; j<Nspecies; j++) {
      rcuts_[i][j] = rcuts[idx];
      idx++;
    }
  }
}

void Descriptor::add_descriptor(char* name, double* values, int row, int col)
{
	double ** params = 0;
	AllocateAndInitialize2DArray(params, row, col);
  int idx = 0;
	for (int i=0; i<row; i++) {
		for (int j=0; j<col; j++) {
			params[i][j] = values[idx];
      idx++;
		}
	}

  int index = 0;
  for (size_t i=0; i<num_param_sets_.size(); i++) {
    index += num_param_sets_[i];
  }

	name_.push_back(name);
	params_.push_back(params);
	num_param_sets_.push_back(row);
	num_params_.push_back(col);
	starting_index_.push_back(index);

  // set t
  if (strcmp(name, "g4") == 0 || strcmp(name, "g5") ==0 ) {
    has_three_body_ = true;
  }

}

int Descriptor::get_num_descriptors() {
  int N = 0;
  for (size_t i=0; i<num_param_sets_.size(); i++) {
    N += num_param_sets_.at(i);
  }
  return N;
}


/*void ggc(double* gen_coords, double* d_gen_coords) {

    std::cout<<"flag-hello"<<std::endl;
    for (int i=0; i<8; i++) {
      std::cout<<i<<std::endl;
    }
}
 */

void Descriptor::get_generalized_coords(double* coords, int* particleSpecies,
    int* neighlist, int* numneigh, int* image, int Natoms, int Ncontrib,
    int Ndescriptor, double* gen_coords, double* d_gen_coords) {

  // prepare data
  VectorOfSizeDIM* coordinates = (VectorOfSizeDIM*) coords;

  int start = 0;
  for (int i=0; i<Ncontrib; i++) {

    int const numNei = numneigh[i];
    int const * const n1Atom = &neighlist[start];
    start += numNei;
    int const iSpecies = particleSpecies[i];

    // Setup loop over neighbors of current particle
    for (int jj = 0; jj < numNei; ++jj)
    {
      // adjust index of particle neighbor
      int const j = n1Atom[jj];
      int const jSpecies = particleSpecies[j];
      double rij[DIM];

      // Compute rij
      for (int dim = 0; dim < DIM; ++dim) {
        rij[dim] = coordinates[j][dim] - coordinates[i][dim];
      }

      // compute distance squared
      double const rijmag = sqrt(rij[0]*rij[0] + rij[1]*rij[1] + rij[2]*rij[2]);
      double const rcutij = rcuts_[iSpecies][jSpecies];

      // if particles i and j not interact
      if (rijmag > rcutij) continue;

      // two-body descriptors
      for (size_t p=0; p<name_.size(); p++) {

        if (name_[p] != "g1" &&
            name_[p] != "g2" &&
            name_[p] != "g3") {
          continue;
        }
        int idx = starting_index_[p];

        for(int q=0; q<num_param_sets_[p]; q++) {

          double gc;
          double dgcdr_two;
          if (name_[p] == "g1") {
            sym_d_g1(rijmag, rcutij, gc, dgcdr_two);
          }
          else if (name_[p] == "g2") {
            double eta = params_[p][q][0];
            double Rs = params_[p][q][1];
            sym_d_g2(eta, Rs, rijmag, rcutij, gc, dgcdr_two);
          }
          else if (name_[p] == "g3") {
            double kappa = params_[p][q][0];
            sym_d_g3(kappa, rijmag, rcutij, gc, dgcdr_two);
          }

          // generalzied coords and derivative
          gen_coords[i*Ndescriptor+idx] += gc;
          for (int kdim = 0; kdim < DIM; ++kdim) {
            double pair = dgcdr_two*rij[kdim]/rijmag;
            int page = (i*Ndescriptor + idx)*DIM*Ncontrib;
            d_gen_coords[page + i*DIM+kdim] += pair;
            d_gen_coords[page + image[j]*DIM+kdim] -= pair;
          }
          idx += 1;

        } // loop over same descriptor but different parameter set
      } // loop over descriptors


      // three-body descriptors
      if (has_three_body_ == false) continue;

      for (int kk = jj+1; kk < numNei; ++kk) {

        // adjust index of particle neighbor
        int const k = n1Atom[kk];
        int const kSpecies = particleSpecies[k];

        // Compute rik, rjk and their squares
        double rik[DIM];
        double rjk[DIM];
        for (int dim = 0; dim < DIM; ++dim) {
          rik[dim] = coordinates[k][dim] - coordinates[i][dim];
          rjk[dim] = coordinates[k][dim] - coordinates[j][dim];
        }
        double const rikmag = sqrt(rik[0]*rik[0] + rik[1]*rik[1] + rik[2]*rik[2]);
        double const rjkmag = sqrt(rjk[0]*rjk[0] + rjk[1]*rjk[1] + rjk[2]*rjk[2]);
        double const rcutik = rcuts_[iSpecies][kSpecies];
        double const rcutjk = rcuts_[jSpecies][kSpecies];

        double const rvec[3] = {rijmag, rikmag, rjkmag};
        double const rcutvec[3] = {rcutij, rcutik, rcutjk};

        if (rikmag > rcutik) continue; // three-dody not interacting

        for (size_t p=0; p<name_.size(); p++) {

          if (name_[p] != "g4" &&
              name_[p] != "g5") {
            continue;
          }
          int idx = starting_index_[p];

          for(int q=0; q<num_param_sets_[p]; q++) {

            double gc;
            double dgcdr_three[3];
            if (name_[p] == "g4") {
              double zeta = params_[p][q][0];
              double lambda = params_[p][q][1];
              double eta = params_[p][q][2];
              sym_d_g4(zeta, lambda, eta, rvec, rcutvec, gc, dgcdr_three);
            }
            else if (name_[p] == "g5") {
              double zeta = params_[p][q][0];
              double lambda = params_[p][q][1];
              double eta = params_[p][q][2];
              sym_d_g5(zeta, lambda, eta, rvec, rcutvec, gc, dgcdr_three);
            }

            // generalzied coords and derivatives
            gen_coords[i*Ndescriptor+idx] += gc;
            int page = (i*Ndescriptor + idx)*DIM*Ncontrib;
            for (int kdim = 0; kdim < DIM; ++kdim) {
              double pair_ij = dgcdr_three[0]*rij[kdim]/rijmag;
              double pair_ik = dgcdr_three[1]*rik[kdim]/rikmag;
              double pair_jk = dgcdr_three[2]*rjk[kdim]/rjkmag;
              d_gen_coords[page + i*DIM+kdim] += pair_ij + pair_ik;
              d_gen_coords[page + image[j]*DIM+kdim] += -pair_ij + pair_jk;
              d_gen_coords[page + image[k]*DIM+kdim] += -pair_ik - pair_jk;
            }
            idx += 1;

          } // loop over same descriptor but different parameter set
        }  // loop over descriptors
      }  // loop over kk (three body neighbors)
    }  // loop over first neighbor
  }  // loop over i atoms


}










//*****************************************************************************
// Symmetry functions: Jorg Behler, J. Chem. Phys. 134, 074106, 2011.
//*****************************************************************************

void Descriptor::sym_g1(double r, double rcut, double &phi) {
  phi = cutoff_(r, rcut);
}

void Descriptor::sym_d_g1(double r, double rcut, double &phi, double &dphi) {
  phi = cutoff_(r, rcut);
  dphi = d_cutoff_(r, rcut);
}

void Descriptor::sym_g2(double eta, double Rs, double r, double rcut, double &phi) {
  phi = exp(-eta*(r-Rs)*(r-Rs)) * cutoff_(r, rcut);
}

void Descriptor::sym_d_g2(double eta, double Rs, double r, double rcut,
    double &phi, double &dphi)
{
  double eterm = exp(-eta*(r-Rs)*(r-Rs));
  double determ = -2*eta*(r-Rs)*eterm;
  double fc = cutoff_(r, rcut);
  double dfc = d_cutoff_(r, rcut);

  phi = eterm*fc;
  dphi = determ*fc + eterm*dfc;
}

void Descriptor::sym_g3(double kappa, double r, double rcut, double &phi) {
	phi = cos(kappa*r) * cutoff_(r, rcut);
}

void Descriptor::sym_d_g3(double kappa, double r, double rcut, double &phi,
    double &dphi)
{
  double costerm = cos(kappa*r);
  double dcosterm = -kappa*sin(kappa*r);
  double fc = cutoff_(r, rcut);
  double dfc = d_cutoff_(r, rcut);

	phi = costerm*fc;
	dphi = dcosterm*fc + costerm*dfc;
}

void Descriptor::sym_g4(double zeta, double lambda, double eta,
    const double* r, const double* rcut, double &phi)
{
  double rij = r[0];
  double rik = r[1];
  double rjk = r[2];
  double rcutij = rcut[0];
  double rcutik = rcut[1];
  double rcutjk = rcut[2];
  double rijsq = rij*rij;
  double riksq = rik*rik;
  double rjksq = rjk*rjk;

  // i is the apex atom
  double cos_ijk = (rijsq + riksq - rjksq)/(2*rij*rik);
  double costerm = pow(1+lambda*cos_ijk, zeta);
  double eterm = exp(-eta*(rijsq + riksq + rjksq));

  phi = pow(2, 1-zeta) * costerm * eterm * cutoff_(rij, rcutij)
      *cutoff_(rik, rcutik) * cutoff_(rjk, rcutjk);
}

void Descriptor::sym_d_g4(double zeta, double lambda, double eta,
    const double* r, const double* rcut, double &phi, double* const dphi)
{
  double rij = r[0];
  double rik = r[1];
  double rjk = r[2];
  double rcutij = rcut[0];
  double rcutik = rcut[1];
  double rcutjk = rcut[2];
  double rijsq = rij*rij;
  double riksq = rik*rik;
  double rjksq = rjk*rjk;


  // cosine term, i is the apex atom
  double cos_ijk = (rijsq + riksq - rjksq)/(2*rij*rik);
  double costerm = pow(1+lambda*cos_ijk, zeta);
  double dcos_dij = (rijsq - riksq + rjksq)/(2*rijsq*rik);
  double dcos_dik = (riksq - rijsq + rjksq)/(2*rij*riksq);
  double dcos_djk = -rjk/(rij*rik);
  double dcosterm_dcos = zeta * pow(1+lambda*cos_ijk, zeta-1) * lambda;
  double dcosterm_dij = dcosterm_dcos * dcos_dij;
  double dcosterm_dik = dcosterm_dcos * dcos_dik;
  double dcosterm_djk = dcosterm_dcos * dcos_djk;

  // exponential term
  double eterm = exp(-eta*(rijsq + riksq + rjksq));
  double determ_dij = -2*eterm*eta*rij;
  double determ_dik = -2*eterm*eta*rik;
  double determ_djk = -2*eterm*eta*rjk;

  // power 2 term
  double p2 = pow(2, 1-zeta);

  // cutoff
  double fcij = cutoff_(rij, rcutij);
  double fcik = cutoff_(rik, rcutik);
  double fcjk = cutoff_(rjk, rcutjk);
  double fcprod = fcij*fcik*fcjk;
  double dfcprod_dij = d_cutoff_(rij, rcutij)*fcik*fcjk;
  double dfcprod_dik = d_cutoff_(rik, rcutik)*fcij*fcjk;
  double dfcprod_djk = d_cutoff_(rjk, rcutjk)*fcij*fcik;

  // phi
  phi =  p2 * costerm * eterm * fcprod;
  // dphi_dij
  dphi[0] = p2 * (dcosterm_dij*eterm*fcprod + costerm*determ_dij*fcprod
      + costerm*eterm*dfcprod_dij);
  // dphi_dik
  dphi[1] = p2 * (dcosterm_dik*eterm*fcprod + costerm*determ_dik*fcprod
      + costerm*eterm*dfcprod_dik);
  // dphi_djk
  dphi[2] = p2 * (dcosterm_djk*eterm*fcprod + costerm*determ_djk*fcprod
      + costerm*eterm*dfcprod_djk);
}

void Descriptor::sym_g5(double zeta, double lambda, double eta,
    const double* r, const double* rcut, double &phi)
{
  double rij = r[0];
  double rik = r[1];
  double rjk = r[2];
  double rcutij = rcut[0];
  double rcutik = rcut[1];
  double rijsq = rij*rij;
  double riksq = rik*rik;
  double rjksq = rjk*rjk;

  // i is the apex atom
  double cos_ijk = (rijsq + riksq - rjksq)/(2*rij*rik);
  double costerm = pow(1+lambda*cos_ijk, zeta);
  double eterm = exp(-eta*(rijsq + riksq));

  phi = pow(2, 1-zeta)*costerm*eterm*cutoff_(rij, rcutij)*cutoff_(rik, rcutik);
}

void Descriptor::sym_d_g5(double zeta, double lambda, double eta,
    const double* r, const double* rcut, double &phi, double* const dphi)
{
  double rij = r[0];
  double rik = r[1];
  double rjk = r[2];
  double rcutij = rcut[0];
  double rcutik = rcut[1];
  double rijsq = rij*rij;
  double riksq = rik*rik;
  double rjksq = rjk*rjk;

  // cosine term, i is the apex atom
  double cos_ijk = (rijsq + riksq - rjksq)/(2*rij*rik);
  double costerm = pow(1+lambda*cos_ijk, zeta);
  double dcos_dij = (rijsq - riksq + rjksq)/(2*rijsq*rik);
  double dcos_dik = (riksq - rijsq + rjksq)/(2*rij*riksq);
  double dcos_djk = -rjk/(rij*rik);
  double dcosterm_dcos = zeta * pow(1+lambda*cos_ijk, zeta-1) * lambda;
  double dcosterm_dij = dcosterm_dcos * dcos_dij;
  double dcosterm_dik = dcosterm_dcos * dcos_dik;
  double dcosterm_djk = dcosterm_dcos * dcos_djk;

  // exponential term
  double eterm = exp(-eta*(rijsq + riksq));
  double determ_dij = -2*eterm*eta*rij;
  double determ_dik = -2*eterm*eta*rik;

  // power 2 term
  double p2 = pow(2, 1-zeta);

  // cutoff
  double fcij = cutoff_(rij, rcutij);
  double fcik = cutoff_(rik, rcutik);
  double fcprod = fcij*fcik;
  double dfcprod_dij = d_cutoff_(rij, rcutij)*fcik;
  double dfcprod_dik = d_cutoff_(rik, rcutik)*fcij;

  // phi
  phi =  p2 * costerm * eterm * fcprod;
  // dphi_dij
  dphi[0] = p2 * (dcosterm_dij*eterm*fcprod + costerm*determ_dij*fcprod
      + costerm*eterm*dfcprod_dij);
  // dphi_dik
  dphi[1] = p2 * (dcosterm_dik*eterm*fcprod + costerm*determ_dik*fcprod
      + costerm*eterm*dfcprod_dik);
  // dphi_djk
  dphi[2] = p2*dcosterm_djk*eterm*fcprod;
}


// helper
// allocate memory and set pointers
void AllocateAndInitialize2DArray(double**& arrayPtr, int const extentZero,
    int const extentOne)
{
  arrayPtr = new double*[extentZero];
  arrayPtr[0] = new double[extentZero * extentOne];
  for (int i = 1; i < extentZero; ++i) {
    arrayPtr[i] = arrayPtr[i-1] + extentOne;
  }
  // initialize
  for (int i = 0; i < extentZero; ++i) {
    for (int j = 0; j < extentOne; ++j) {
      arrayPtr[i][j] = 0.0;
    }
  }
}

// deallocate memory
void Deallocate2DArray(double**& arrayPtr) {
  if (arrayPtr != 0) delete [] arrayPtr[0];
  delete [] arrayPtr;

  // nullify pointer
  arrayPtr = 0;
}


