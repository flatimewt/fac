#include "radial.h"

static POTENTIAL *potential;

static ARRAY *orbitals;
static int n_orbitals;
static int n_continua;
 
static double _dwork[MAX_POINTS];
static double _dwork1[MAX_POINTS];
static double _dwork2[MAX_POINTS];
static double _dwork3[MAX_POINTS];
static double _dwork4[MAX_POINTS];
static double _dwork5[MAX_POINTS];
static double _dwork6[MAX_POINTS];
static double _dwork7[MAX_POINTS];
static double _dwork8[MAX_POINTS];
static double _dwork9[MAX_POINTS];
static double _dwork10[MAX_POINTS];
static double _dwork11[MAX_POINTS];
static double _phase[MAX_POINTS];
static double _dphase[MAX_POINTS];
static double _yk[MAX_POINTS];
static double _zk[MAX_POINTS];
static double _xk[MAX_POINTS];

static struct {
  double tolerence; /* tolerence for self-consistency */
  int maxiter; /* max iter. for self-consistency */
  int screening; /* after which n, additional screening of one more e-*/
  int iprint; /* printing infomation in each iteration. */
} optimize_controll = {EPS6, 100, 8, 0};

static double rgrid_min = 1E-5;
static double rgrid_max = 0.5E4;    

static RAD_TIMING rad_timing = {0, 0};
 
static MULTI *slater_array;
static MULTI *residual_array;
static MULTI *multipole_array;  

int GetRadTiming(RAD_TIMING *t) {
  memcpy(t, &rad_timing, sizeof(RAD_TIMING));
  return 0;
}

void SetOptimizeControll(double tolerence, int maxiter, int iprint) {
  optimize_controll.maxiter = maxiter;
  optimize_controll.tolerence = tolerence;
  optimize_controll.iprint = iprint;  
}

void SetScreening(int n) {
  optimize_controll.screening = n;
}

int SetRadialGrid(double rmin, double rmax) {
  rgrid_min = rmin;
  rgrid_max = rmax;
}

int _AdjustScreeningParams(double *v, double *u) {
  int i;
  double a, b, c;

  for (i = 0; i < MAX_POINTS; i++) {
    u[i] = 0.5*(u[i]+v[i]);
    v[i] = u[i];
  }
  c = 0.5*u[MAX_POINTS-1];
  for (i = 0; i < MAX_POINTS; i++) {
    if (u[i] > c) break;
  }
  potential->lambda = log(2.0)/potential->rad[i];
  return 0;
}
  
int SetPotential(AVERAGE_CONFIG *acfg) {
  int i, j, k1, k2, k, t, m, j1, j2, kl1, kl2;
  ORBITAL *orb1, *orb2;
  double large1, small1, large2, small2;
  int norbs, kmin, kmax, jmax;
  double *u, *w, *v, w3j, a, b;

  u = potential->U;
  w = potential->W;
  v = potential->Vtail;

  for (j = 0; j < MAX_POINTS; j++) {
    w[j] = 0.0;
  }

  norbs = 0;
  jmax = 0;
  for (i = 0; i < acfg->n_shells; i++) {
    k1 = OrbitalExists(acfg->n[i], acfg->kappa[i], 0.0);
    if (k1 < 0) continue;
    orb1 = GetOrbital(k1);
    for (j = 0; j <= orb1->ilast; j++) {
      large1 = Large(orb1)[j];
      small1 = Small(orb1)[j];
      w[j] += (large1*large1 + small1*small1)*acfg->nq[i];
    }
    if (jmax < orb1->ilast) jmax = orb1->ilast;
    norbs++;
  }

  if (norbs && (potential->N > 1)) {
    for (j = 0; j < MAX_POINTS; j++) {
      u[j] = 0.0;
    }
    for (i = 0; i < acfg->n_shells; i++) {
      k1 = OrbitalExists(acfg->n[i], acfg->kappa[i], 0.0);
      if (k1 < 0) continue;
      orb1 = GetOrbital(k1);
      GetJLFromKappa(acfg->kappa[i], &j1, &kl1);
      kmin = 0;
      kmax = 2*j1;
      kmax = Min(kmax, 8);
      for (k = kmin; k <= kmax; k += 2) {
	t = k/2;
	if (IsOdd(t)) continue;
	GetYk(t, _yk, orb1, orb1, -1);
	if (t > 0) {
	  w3j = W3j(j1, k, j1, -1, 0, -1);
	  w3j *= w3j*(j1+1.0)/j1;
	}
	for (m = 1; m <= jmax; m++) {
	  large1 = Large(orb1)[m];
	  small1 = Small(orb1)[m];
	  b = large1*large1 + small1*small1;
	  if (t == 0) {	  
	    u[m] += acfg->nq[i]*_yk[m];
	    a = _yk[m]*b*acfg->nq[i];
	    a /= w[m];
	    u[m] -= a;
	  } else {
	    a = acfg->nq[i]*(acfg->nq[i]-1.0);
	    a *= w3j*_yk[m]*b;
	    a /= w[m];
	    u[m] -= a;
	  }
	}
      }
      for (j = 0; j < i; j++) {
	k2 = OrbitalExists(acfg->n[j], acfg->kappa[j], 0.0);
	if (k2 < 0) continue;
	orb2 = GetOrbital(k2);
	GetJLFromKappa(acfg->kappa[j], &j2, &kl2);
	kmin = abs(j1 - j2);
	kmax = j1 + j2;
	kmax = Min(kmax, 8);
	if (IsOdd(kmin)) kmin++;
	for (k = kmin; k <= kmax; k += 2) {
	  if (IsOdd((k+kl1+kl2)/2)) continue;
	  t = k/2;
	  GetYk(t, _yk, orb1, orb2, -1);
	  w3j = W3j(j1, k, j2, -1, 0, -1);
	  w3j *= w3j;
	  for (m = 1; m <= jmax; m++) {
	    large1 = Large(orb1)[m];
	    large2 = Large(orb2)[m];
	    small1 = Small(orb1)[m];
	    small2 = Small(orb2)[m];
	    a = acfg->nq[i]*acfg->nq[j];
	    a *= w3j*_yk[m]*(large1*large2+small1*small2);
	    a /= w[m];
	    u[m] -= a;
	  }
	}
      }
    }

    u[0] = u[1];
    for (j = jmax+1; j < MAX_POINTS; j++) {
      u[j] = u[jmax];
    }
    _AdjustScreeningParams(v, u);
    SetPotentialVc(potential);
    for (j = 0; j < MAX_POINTS; j++) {
      u[j] = u[j] - potential->Z[j];
      u[j] -= potential->Vc[j]*potential->rad[j];
      u[j] /= potential->rad[j];
    }
    SetPotentialU(potential, 0, NULL);
  } else {
    for (j = 0; j < MAX_POINTS; j++) {
      v[j] = (potential->U[i] + potential->Vc[i])*potential->rad[i];
      v[j] += potential->Z[MAX_POINTS-1];
    }
    SetPotentialVc(potential);
    SetPotentialU(potential, -1, NULL);
  }
  
  return 0;
}

double GetResidualZ(int m) {
  double z;
  z = potential->Z[MAX_POINTS-1] - potential->N + 1;
  if (m) z -= 1; 
  return z;
}

double GetRMax() {
  return potential->rad[MAX_POINTS-1];
}
  
int OptimizeRadial(int ng, int *kg, double *weight) {
  AVERAGE_CONFIG *acfg;
  AVERAGE_CONFIG average_config;
  double tol;
  ORBITAL orb_old, *orb;
  int i, j, k, no_old;
  double a, b, maxp;
  double z, rmax, lambda, step, q;
  double large, large_old;
  int iter;

  FreeSlaterArray();
  FreeResidualArray();

  /* get the average configuration for the groups */
  acfg = &(average_config);
  GetAverageConfig(ng, kg, weight, acfg); 
  a = 0;
  for (i = 0; i < acfg->n_shells; i++) {
    a += acfg->nq[i];
  }
  potential->N = a;  

  /* setup the radial grid if not yet */
  if (potential->flag == 0) { 
    SetOrbitalRGrid(potential, rgrid_min, rgrid_max);
  }
  SetPotentialZ(potential, 0.0);
  z = potential->Z[MAX_POINTS-1];
  if (a > 0.0) z = z - a + 1;
  potential->a = 0.0;
  potential->lambda = 0.5*z;

  no_old = 0;  
  tol = 1.0; 
  iter = 0;  

  orb_old.wfun = malloc(sizeof(double)*MAX_POINTS);
  if (!(orb_old.wfun)) return -1;

  if(a > 2*z) z = a/potential->Z[MAX_POINTS-1];
  else z = 0.0;
  while (tol > optimize_controll.tolerence || z > 0.0) {
    if (iter > optimize_controll.maxiter) break;
    if (z < 1E-3 && z > 0.0) {
      z = 0.0;
      SetPotentialZ(potential, 0.0);
    } else {
      SetPotentialZ(potential, z);
      z *= 0.5;
    }
    SetPotential(acfg);
    tol = 0.0;
    for (i = 0; i < acfg->n_shells; i++) {
      k = OrbitalExists(acfg->n[i], acfg->kappa[i], 0.0);
      if (k < 0) {
	orb_old.energy = 0.0;
	orb = GetNewOrbital();
	orb->kappa = acfg->kappa[i];
	orb->n = acfg->n[i];
	orb->energy = 0.0;
	no_old = 1;
      } else {
	orb = GetOrbital(k);
	orb_old.energy = orb->energy; 
	orb_old.ilast = orb->ilast;
	memcpy(orb_old.wfun, orb->wfun, sizeof(double)*MAX_POINTS);
	free(orb->wfun);
	no_old = 0;
      }
 
      if (SolveDirac(orb) < 0) {
	return -1;
      }

      if (no_old) { 
	tol = 1.0;
	continue;
      } 
      
      maxp = 0.0;
      b = 0.0;
      for (j = 0; j <= Min(orb_old.ilast, orb->ilast); j++) {
	large = orb->wfun[j];
	a = fabs(large);
	if (a > maxp) maxp = a;
	large_old = orb_old.wfun[j];
	a = fabs(large - large_old);
	if (a > b) b = a;
      }
      b = b/maxp;
      a = fabs(1.0 - orb_old.energy/orb->energy);
      b = Max(a, b);
      b = a;
      if (tol < b) tol = b;
    }
    if (optimize_controll.iprint) {
      printf("%2d %18.10lE %10.3E\n", iter, tol, z);
    }
    iter++;
  }
  free(orb_old.wfun);

  if (iter > optimize_controll.maxiter) {
    printf("Maximum iteration reached in OptimizeRadial\n");
    return 1;
  }

  a = optimize_controll.screening;
  a = a*a;
  if (a > 0) {
    if (a < 10) a = 10.0;
    potential->lambdap = potential->lambda / a;
    potential->ap = potential->a / a;
  } else {
    potential->lambdap = 0.0;
    potential->ap = 0.0;
  }
  SetPotentialVTail(potential);

  return 0;
}      
    
int SolveDirac(ORBITAL *orb) {
  double z;
  double eps;
  int i, j;
  int err;
  clock_t start, stop;

#ifdef PERFORM_STATISTICS
  start = clock();
#endif
  
  err = 0;
  eps = optimize_controll.tolerence*1E-1;
  potential->flag = -1;
  if (optimize_controll.screening > 0 &&
      (orb->n <= 0 || orb->n >= optimize_controll.screening)) 
    potential->flag = -2;

  err = RadialSolver(orb, potential, eps);
  if (err) { 
    printf("Error ocuured in RadialSolver, %d\n", err);
    printf("%d %d %10.3E\n", orb->n, orb->kappa, orb->energy);
    abort();
  }
#ifdef PERFORM_STATISTICS
  stop = clock();
  rad_timing.dirac += stop - start;
#endif

  return err;
}

int WaveFuncTable(char *s, int n, int kappa, double e) {
  int i, k;
  FILE *f;
  ORBITAL *orb;

  e /= HARTREE_EV;
  k = OrbitalIndex(n, kappa, e);
  if (k < 0) return -1;
  f = fopen(s, "w");
  if (!f) return -1;
  
  orb = GetOrbital(k);
  
  fprintf(f, "#Wave Function for n = %d, kappa = %d, energy = %12.6E\n\n",
	  n, kappa, orb->energy*HARTREE_EV);
  if (n > 0) {
    for (i = 0; i <= orb->ilast; i++) {
      fprintf(f, "%-4d %10.3E %10.3E %10.3E %10.3E %10.3E\n", 
	      i, potential->rad[i], 
	      (potential->Vc[i])*potential->rad[i],
	      potential->U[i] * potential->rad[i],
	      Large(orb)[i], Small(orb)[i]); 
    }
  } else {
    for (i = 0; i <= orb->ilast; i++) {
      fprintf(f, "%-4d %10.3E %10.3E %10.3E %10.3E %10.3E\n", 
	      i, potential->rad[i],
	      (potential->Vc[i])*potential->rad[i],
	      potential->U[i] * potential->rad[i],
	      Large(orb)[i], Small(orb)[i]); 
    }
    for (; i < MAX_POINTS; i += 2) {
      fprintf(f, "%-4d %10.3E %10.3E %10.3E %10.3E %10.3E\n",
	      potential->rad[i],
	      Large(orb)[i], Large(orb)[i+1], 
	      Small(orb)[i], Small(orb)[i+1]);
    }
  }

  fclose(f);
}

double GetPhaseShift(int k, int mode) {
  ORBITAL *orb;
  double phase, r, y, z, ke, e, a;
  int i;

  orb = GetOrbital(k);
  if (orb->n > 0) return 0.0;

  if (orb->phase >= 0.0) return orb->phase;

  z = GetResidualZ(1);
  e = orb->energy;
  a = FINE_STRUCTURE_CONST2 * e;
  ke = sqrt(2.0*e*(1.0 + 0.5*e));
  y = (1.0 + a)*z/ke;

  i = MAX_POINTS - 1;
  
  phase = orb->wfun[i];
  r = potential->rad[i];
  
  a = ke * r;
  
  phase -= a + y*log(2.0*a);
  
  a = phase/TWO_PI;
  r = floor(a);
  phase = (a-r)*TWO_PI;
  orb->phase = phase;

  return phase;  
}

int GetNumBounds() {
  return n_orbitals - n_continua;
}

int GetNumOrbitals() {
  return n_orbitals;
}

int GetNumContinua() {
  return n_continua;
}

int OrbitalIndex(int n, int kappa, double energy) {
  int i, j;
  ORBITAL *orb;
  int resolve_dirac;

  resolve_dirac = 0;
  for (i = 0; i < n_orbitals; i++) {
    orb = GetOrbital(i);
    if (n == 0) {
      if (orb->kappa == kappa && 
	  orb->energy > 0.0 &&
	  fabs(orb->energy - energy) < EPS6) {
	if (orb->wfun == NULL) {
	  if (RestoreOrbital(i) == 0) return i;
	  else {
	    resolve_dirac = 1;
	    break;
	  }
	}
	return i;
      }
    } else if (orb->n == n && orb->kappa == kappa) {
      if (orb->wfun == NULL) {
	if (RestoreOrbital(i) == 0) return i;
	else {
	  resolve_dirac = 1;
	  break;
	}
      }
      return i;
    }
  }
  
  if (!resolve_dirac) {
    orb = GetNewOrbital();
  } 

  orb->n = n;
  orb->kappa = kappa;
  orb->energy = energy;
  j = SolveDirac(orb);
  if (j < 0) {
    printf("Error occured in solving Dirac eq. err = %d\n", j);
    abort();
  }
  
  if (n == 0 && !resolve_dirac) {
    n_continua++;
    orb->n = -n_continua;
  }
  return i;
}

int OrbitalExists(int n, int kappa, double energy) {
  int i;
  ORBITAL *orb;
  for (i = 0; i < n_orbitals; i++) {
    orb = GetOrbital(i);
    if (n == 0) {
      if (orb->kappa == kappa &&
	  fabs(orb->energy - energy) < EPS6) 
	return i;
    } else if (orb->n == n &&
	       orb->kappa == kappa)
      return i;
  }
  return -1;
}

int AddOrbital(ORBITAL *orb) {
  int j;

  if (orb == NULL) return -1;

  orb = (ORBITAL *) ArrayAppend(orbitals, orb);
  if (!orb) {
    printf("Not enough memory for orbitals array\n");
    abort();
  }

  if (orb->n == 0) {
    n_continua++;
    orb->n = -n_continua;
  }
  n_orbitals++;
  return n_orbitals - 1;
}

ORBITAL *GetOrbital(int k) {
  return (ORBITAL *) ArrayGet(orbitals, k);
}

ORBITAL *GetNewOrbital() {
  int j;
  ORBITAL *orb;

  orb = (ORBITAL *) ArrayAppend(orbitals, NULL);
  if (!orb) {
    printf("Not enough memory for orbitals array\n");
    abort();
  }

  orb->wfun = NULL;

  n_orbitals++;
  return orb;
}

int SaveOrbital(int i) {
  return 0;
}

int RestoreOrbital(int i) {
  return -1;
}

int FreeOrbital(int i) {
  ORBITAL *orb;
  orb = GetOrbital(i);
  if (orb->wfun) free(orb->wfun);
  orb->wfun = NULL;
}

int SaveAllContinua(int mode) {
  int i;
  ORBITAL *orb;
  for (i = 0; i < n_orbitals; i++) {
    orb = GetOrbital(i);
    if (orb->n <= 0 && orb->wfun != NULL) {
      if (SaveOrbital(i) < 0) return -1;
      if (mode) {
	FreeOrbital(i);
      }
    }
  }
  return 0;
}

int SaveContinua(double e, int mode) {
  int i;
  ORBITAL *orb;
  for (i = 0; i < n_orbitals; i++) {
    orb = GetOrbital(i);
    if (orb->n <= 0 && 
	orb->wfun != NULL &&
	fabs(orb->energy - e) < EPS3) {
      if (SaveOrbital(i) < 0) return -1;
      if (mode) FreeOrbital(i);
    }
  }
  return 0;
}

int FreeAllContinua() {
  int i;
  ORBITAL *orb;
  for (i = 0; i < n_orbitals; i++) {
    orb = GetOrbital(i);
    if (orb->n <= 0 && orb->wfun != NULL) {
      FreeOrbital(i);
    }
  }
  return 0;
}

int FreeContinua(double e) {
  int i;
  ORBITAL *orb;
  for (i = 0; i < n_orbitals; i++) {
    orb = GetOrbital(i);
    if (orb->n <= 0 && 
	orb->wfun != NULL &&
	fabs(orb->energy - e) < EPS3) {
      FreeOrbital(i);
    }
  }
  return 0;
}

/* calculate the total configuration average energy of a group. */
double TotalEnergyGroup(int kg) {
  CONFIG_GROUP *g;
  ARRAY *c;
  CONFIG *cfg;
  ORBITAL *orb1, *orb2;
  int t;
  double total_energy;

  g = GetGroup(kg);
  c = &(g->cfg_list);
  
  total_energy = 0.0;
  for (t = 0; t < g->n_cfgs; t++) {
    cfg = (CONFIG *) ArrayGet(c, t);
    total_energy += AverageEnergyConfig(cfg);
  }
  return total_energy;
}

/* calculate the average energy of a configuration */
double AverageEnergyConfig(CONFIG *cfg) {
  int i, j, n, kappa, nq, np, kappap, nqp;
  int k, kp, kk, kl, klp, kkmin, kkmax, j2, j2p;
  double x, y, t, q, a, b, r;
 
  x = 0.0;
  for (i = 0; i < cfg->n_shells; i++) {
    n = (cfg->shells[i]).n;
    kappa = (cfg->shells[i]).kappa;
    kl = GetLFromKappa(kappa);
    j2 = GetJFromKappa(kappa);
    nq = (cfg->shells[i]).nq;
    k = OrbitalIndex(n, kappa, 0.0);
    
    if (nq > 1) {
      t = 0.0;
      for (kk = 2; kk <= j2; kk += 2) {
	Slater(&y, k, k, k, k, kk, 0);
	q = W3j(j2, 2*kk, j2, -1, 0, 1);
	t += y * q * q ;
      }
      Slater(&y, k, k, k, k, 0, 0);
      b = ((nq-1.0)/2.0) * (y - (1.0 + 1.0/j2)*t);

#if FAC_DEBUG
      fprintf(debug_log, "\nAverage Radial: %lf\n", y);
#endif

    } else b = 0.0;
    t = 0.0;
    for (j = 0; j < i; j++) {
      np = (cfg->shells[j]).n;
      kappap = (cfg->shells[j]).kappa;
      klp = GetLFromKappa(kappap);
      j2p = GetJFromKappa(kappap);
      nqp = (cfg->shells[j]).nq;
      kp = OrbitalIndex(np, kappap, 0.0);

      kkmin = abs(j2 - j2p);
      kkmax = (j2 + j2p);
      if (IsOdd((kkmin + kl + klp)/2)) kkmin += 2;
      a = 0.0;
      for (kk = kkmin; kk <= kkmax; kk += 4) {
	Slater(&y, k, kp, kp, k, kk/2, 0);
	q = W3j(j2, kk, j2p, -1, 0, 1);
	a += y * q * q;

#if FAC_DEBUG
	fprintf(debug_log, "exchange rank: %d, q*q: %lf, Radial: %lf\n", 
		kk/2, q*q, y);
#endif

      }
      Slater(&y, k, kp, k, kp, 0, 0);

#if FAC_DEBUG
      fprintf(debug_log, "direct: %lf\n", y);
#endif

      t += nqp * (y - a);
    }

    ResidualPotential(&y, k, k);

    r = nq * (b + t + GetOrbital(k)->energy + y);
    x += r;
  }
  return x;
}

/* calculate the expectation value of the residual potential:
   -Z/r - v0(r), where v0(r) is central potential used to solve 
   dirac equations. the orbital index must be valid, i.e., their 
   radial equations must have been solved. */
int ResidualPotential(double *s, int k0, int k1) {
  int i;
  ORBITAL *orb1, *orb2;
  int index[2], tail;
  double *p, z;

  if (k0 > k1) {
    index[0] = k1;
    index[1] = k0;
  } else {
    index[0] = k0;
    index[1] = k1;
  }

  p = (double *) MultiSet(residual_array, index, NULL);
  if (*p) {
    *s = *p;
    return 0;
  } 

  *s = 0.0;
  orb1 = GetOrbital(k0);
  orb2 = GetOrbital(k1);
  if (!orb1 || !orb2) return -1;
  if (optimize_controll.screening > 0 &&
      (orb1->n >= optimize_controll.screening || 
       orb2->n >= optimize_controll.screening ||
       orb1->n <= 0 ||
       orb2->n <= 0)) {
    tail = 1;
  } else {
    tail = 0;
  }
 
  for (i = 0; i < MAX_POINTS; i++) {
    z = potential->U[i];
    z += potential->Vc[i];
    if (tail) z += potential->Vtail[i];
    _yk[i] = -(potential->Z[i]/potential->rad[i]) - z;
  }
  Integrate(_yk, orb1, orb2, 1, s);
  
  *p = *s;
  return 0;
}

/* the multipole operator in non-relativistic approximation.
   coulomb gauge corresponding to the velocity form. 
   babushkin gauge to length form. m is the rank. positive means 
   magnetic multipoles, negtive means electric. If it is <=
   -256 or >= 256, however, it calculates the expectation value of
   r^(m+/-256), this is used in the evaluation of slater integral 
   in separable coulomb interaction. */
double MultipoleRadialNR(int m, int k1, int k2) {
  int i, p, t;
  int npts;
  ORBITAL *orb1, *orb2;
  double r;
  int index[4];
  double *q;
  int gauge;
  int kappa1, kappa2;
  clock_t start, stop;

#ifdef PERFORM_STATISTICS 
  start = clock();
#endif

  if (m == 0) return 0.0;

  gauge = TransitionGauge();

  if (m >= 256) {
    gauge = G_BABUSHKIN;
    index[1] = m-256;
    index[0] = 0;
  } else if (m <= -256) {
    gauge = G_BABUSHKIN;
    index[1] = -m-256;
    index[0] = 1;    
  } else if (m > 0) {
    index[1] = m;
    index[0] = (gauge == G_BABUSHKIN)?2:3;
  } else if (m < 0) {
    index[1] = -m;
    index[0] = (gauge == G_BABUSHKIN)?4:5;
  }
  index[2] = k1;
  index[3] = k2;
  
  q = (double *) MultiSet(multipole_array, index, NULL);
  if (*q) {
    return *q;
  }
  
  orb1 = GetOrbital(k1);
  orb2 = GetOrbital(k2);
  kappa1 = orb1->kappa;
  kappa2 = orb2->kappa;
  npts = MAX_POINTS-1;
  if (orb1->n > 0) npts = Min(npts, orb1->ilast);
  if (orb2->n > 0) npts = Min(npts, orb2->ilast);

  r = 0.0;
  if (m > 0 && m < 256) {
    t = kappa1 + kappa2;
    p = m - t;
    if (p && t) {
      for (i = 0; i <= npts; i++) {
	_yk[i] = pow(potential->rad[i], m-1);
      }
      Integrate(_yk, orb1, orb2, 1, &r);
      r *= p*t;
      r /= sqrt(m*(m+1.0));
      r *= -0.5 * FINE_STRUCTURE_CONST;
      for (i = 2*m - 1; i > 0; i -= 2) r /= (double) i;
    }
    r *= ReducedCL(GetJFromKappa(kappa1), 2*m, GetJFromKappa(kappa2));
  } else if (m < 0 && m > -256) {
    m = -m;
    if (gauge == G_BABUSHKIN) {
      for (i = 0; i <= npts; i++) {
	_yk[i] = pow(potential->rad[i], m);
      }
      Integrate(_yk, orb1, orb2, 1, &r);
      r *= sqrt((m+1.0)/m);
      for (i = 2*m - 1; i > 1; i -= 2) r /= (double) i;
    } else {
      /* the velocity form is not implemented yet. 
	 the following is still the length form */
      for (i = 0; i <= npts; i++) { 
	_yk[i] = pow(potential->rad[i], m);
      }
      Integrate(_yk, orb1, orb2, 1, &r);
      r *= sqrt((m+1.0)/m);
      for (i = 2*m - 1; i > 1; i -= 2) r /= (double) i;
    }
    r *= ReducedCL(GetJFromKappa(kappa1), 2*m, GetJFromKappa(kappa2));
  } else {
    m = (m > 0)?(m-256):(m+256);
    for (i = 0; i <= npts; i++) {
      _yk[i] = pow(potential->rad[i], m);
    }
    Integrate(_yk, orb1, orb2, 1, &r);
  }
  
  *q = r;
  
#ifdef PERFORM_STATISTICS 
  stop = clock();
  rad_timing.radial_1e += stop - start;
#endif
  
  return r;
}

/* fully relativistic multipole operator, 
   see Grant, J. Phys. B. 1974. Vol. 7, 1458. */ 
double MultipoleRadial(double aw, int m, int k1, int k2) {
  double r, q, ip, ipm, im, imm;
  int kappa1, kappa2;
  int am, t;
  int index[4];
  double *p;
  ORBITAL *orb1, *orb2;
  int gauge;
  clock_t start, stop;

#ifdef PERFORM_STATISTICS 
  start = clock();
#endif

  if (m == 0) return 0.0;

  gauge = TransitionGauge();
    
  if (m > 0) {
    index[1] = m;
    index[0] = (gauge == G_BABUSHKIN)?6:7; 
  } else if (m < 0) {
    index[1] = -m;
    index[0] = (gauge == G_BABUSHKIN)?8:9;
  }
  index[2] = k1;
  index[3] = k2;

  p = (double *) MultiSet(multipole_array, index, NULL);
  if (*p) {
    return *p;
  }
  
  orb1 = GetOrbital(k1);
  orb2 = GetOrbital(k2);
  kappa1 = orb1->kappa;
  kappa2 = orb2->kappa;
  r = 0.0;
  if (m > 0) {
    t = kappa1 + kappa2;
    if (t) {
      r = t * MultipoleIJ(aw, m, orb1, orb2, 4);
      r *= (2*m + 1.0)/sqrt(m*(m+1.0));
      r /= pow(aw, m);
    }
  } else {
    am = -m;
    if (gauge == G_COULOMB) {
      t = kappa1 - kappa2;
      q = sqrt(am/(am+1.0));
      if (t) {
	ip = MultipoleIJ(aw, am+1, orb1, orb2, 4);
	ipm = MultipoleIJ(aw, am-1, orb1, orb2, 4);
	r = t*ip*q - t*ipm/q;
      }
      im = MultipoleIJ(aw, am+1, orb1, orb2, 5);
      imm = MultipoleIJ(aw, am-1, orb1, orb2, 5);
      r += (am + 1.0)*im*q + am*imm/q;
      r /= pow(aw,am);
    } else if (gauge == G_BABUSHKIN) {
      t = kappa1 - kappa2;
      if (t) {
	ip = MultipoleIJ(aw, am+1, orb1, orb2, 4);
	r = t*ip;
      }
      im = MultipoleIJ(aw, am+1, orb1, orb2, 5);
      imm = MultipoleIJ(aw, am, orb1, orb2, 1);
      r += (am + 1.0) * (imm + im);
      q = (2*am + 1.0)/sqrt(am*(am+1.0));
      r = r*q/pow(aw,am);
    }
  }

  r *= ReducedCL(GetJFromKappa(kappa1), 2*am, GetJFromKappa(kappa2));

  *p = r;

#ifdef PERFORM_STATISTICS 
    stop = clock();
    rad_timing.radial_1e += stop - start;
#endif
  return r;
}

/* calculates the I and J integral defined in Grant. J. Phys. B. 
   V7. 1458 */
double MultipoleIJ(double aw, int m, 
		   ORBITAL *orb1, ORBITAL *orb2, int type) {
  int i, npts;
  double r, x;
  int jy, n;

  jy = 1;
  n = m;
  npts = MAX_POINTS-1;
  if (orb1->n > 0) npts = Min(npts, orb1->ilast);
  if (orb2->n > 0) npts = Min(npts, orb2->ilast);
  for (i = 0; i <= npts; i++) {
    x = aw * potential->rad[i];
    _yk[i] = besljn_(&jy, &n, &x); 
  }
  Integrate(_yk, orb1, orb2, type, &r);
  return r;
} 

int SlaterTotal(double *sd, double *se, int *j, int *ks, int k, int mode) {
  int t, kk, tt;
  int tmin, tmax;
  double e, a, d;
  int err;
  int kl0, kl1, kl2, kl3;
  int k0, k1, k2, k3;
  int js[4];
  ORBITAL *orb0, *orb1, *orb2, *orb3;
  
  k0 = ks[0];
  k1 = ks[1];
  k2 = ks[2];
  k3 = ks[3];
  if (j) {
    memcpy(js, j, sizeof(int)*4);
  } else {
    js[0] = 0;
    js[1] = 0;
    js[2] = 0;
    js[3] = 0;
  }

  kk = k/2;

  orb0 = GetOrbital(k0);
  orb1 = GetOrbital(k1);
  orb2 = GetOrbital(k2);
  orb3 = GetOrbital(k3);
  if (js[0] <= 0) js[0] = GetJFromKappa(orb0->kappa);
  if (js[1] <= 0) js[1] = GetJFromKappa(orb1->kappa);
  if (js[2] <= 0) js[2] = GetJFromKappa(orb2->kappa);
  if (js[3] <= 0) js[3] = GetJFromKappa(orb3->kappa);

  kl0 = GetLFromKappa(orb0->kappa);
  kl1 = GetLFromKappa(orb1->kappa);
  kl2 = GetLFromKappa(orb2->kappa);
  kl3 = GetLFromKappa(orb3->kappa);
  
  if (sd) {
    d = 0.0;
    if (IsEven((kl0+kl2)/2+kk) && IsEven((kl1+kl3)/2+kk) &&
	Triangle(js[0], js[2], k) && Triangle(js[1], js[3], k)) {
      err = Slater(&d, k0, k1, k2, k3, kk, mode);
      if (err < 0) return err;
      d *= ReducedCL(js[0], k, js[2]);
      d *= ReducedCL(js[1], k, js[3]);
      if (k0 == k1 && k2 == k3) d *= 0.5;
    }
    *sd = d;
  }
  
  if (se == NULL) return 0;
  if (abs(mode) == 2) {
    *se = 0.0;
    return 0;
  }
  *se = 0.0;
  if (k0 == k1 && (orb0->n > 0 || orb1->n > 0)) return 0;
  if (k2 == k3 && (orb2->n > 0 || orb3->n > 0)) return 0;
  tmin = abs(js[0] - js[3]);
  tt = abs(js[1] - js[2]);
  tmin = Max(tt, tmin);
  tmax = js[0] + js[3];
  tt = js[1] + js[2];
  tmax = Min(tt, tmax);
  tmax = Min(tmax, GetMaxRank());
  if (IsOdd(tmin)) tmin++;
  
  for (t = tmin; t <= tmax; t += 2) {
    a = W6j(js[0], js[2], k, js[1], js[3], t);
    if (fabs(a) > EPS10 &&
	IsEven((kl0+kl3+t)/2) && IsEven((kl1+kl2+t)/2)) {
      e = 0.0;
      err = Slater(&e, k0, k1, k3, k2, t/2, mode);
      e *= ReducedCL(js[0], t, js[3]);
      e *= ReducedCL(js[1], t, js[2]);
      e *= a * (k + 1.0);
      if (IsOdd(t/2 + kk)) e = -e;
      *se += e;
    }
  }

  return 0;
}

/* calculate the slater integral of rank k */
int Slater(double *s, int k0, int k1, int k2, int k3, int k, int mode) {
  int index[6];
  double *p;
  int ilast, i, j, npts, m;
  ORBITAL *orb0, *orb1, *orb2, *orb3;
  double norm, rk;
  clock_t start, stop;

#ifdef PERFORM_STATISTICS 
  start = clock();
#endif

  index[0] = k0;
  index[1] = k1;
  index[2] = k2;
  index[3] = k3;
  index[4] = k;
  
  switch (mode) {
  case 0:
  case 1:
    index[5] = 0;
    break;
  case 2:
    index[5] = 1;
    break;
  case -1:
    index[5] = 2;
    break;
  case -2:
    index[5] = 3;
    break;
  default:
    printf("mode unrecognized in slater\n");
    return -1;
  }
  SortSlaterKey(index);

  p = (double *) MultiSet(slater_array, index, NULL);
  if (*p) {
    *s = *p;
  } else {
    orb0 = GetOrbital(k0);
    orb1 = GetOrbital(k1);
    orb2 = GetOrbital(k2);
    orb3 = GetOrbital(k3);
    *s = 0.0;
    if (!orb0 || !orb1 || !orb2 || !orb3) return -1;  

    npts = MAX_POINTS;
    switch (mode) {
    case 0: /* fall through to case 1 */
    case 1: /* full relativistic with distorted free orbitals */
      GetYk(k, _yk, orb0, orb2, -1); 
      if (orb1->n > 0) ilast = orb1->ilast;
      else ilast = npts-1;
      if (orb3->n > 0) ilast = Min(ilast, orb3->ilast);
      for (i = 0; i <= ilast; i++) {
	_yk[i] = (_yk[i]/potential->rad[i]);
      }
      Integrate(_yk, orb1, orb3, 1, s);
      break;
    
    case -1: /* quasi relativistic with distorted free orbitals */
      GetYk(k, _yk, orb0, orb2, -2); 
      if (orb1->n > 0) ilast = orb1->ilast;
      else ilast = npts-1;
      if (orb3->n > 0) ilast = Min(ilast, orb3->ilast);
      for (i = 0; i <= ilast; i++) {
	_yk[i] /= potential->rad[i];
      }
      Integrate(_yk, orb1, orb3, 2, s);

      norm  = orb0->qr_norm;
      norm *= orb1->qr_norm;
      norm *= orb2->qr_norm;
      norm *= orb3->qr_norm;
      *s *= norm;
      break;

    case 2: /* separable coulomb interaction, orb0, orb2 is inner part */
      m = k;
      if (m == 0) {
	*s = (k0 == k2)?1.0:0.0;
      } else {
	*s = MultipoleRadialNR(m+256, k0, k2);
      }
      if (*s != 0.0) {
	m = -m-1;
	*s *= MultipoleRadialNR(m-256, k1, k3);
      }
      break;

    case -2: /* separable coulomb interaction, orb1, orb3 is inner part  */
      m = k;
      if (m == 0) {
	*s = (k0 == k2)?1.0:0.0;
      } else {
	*s = MultipoleRadialNR(m+256, k1, k3);
      }
      if (*s != 0.0) {
	m = -m-1;
	*s *= MultipoleRadialNR(m-256, k0, k2);      
      }
      break;
      
    default:
      break;
    }      

    *p = *s;
  }
#ifdef PERFORM_STATISTICS 
    stop = clock();
    rad_timing.radial_2e += stop - start;
#endif
  return 0;
}


/* reorder the orbital index appears in the slater integral, so that it is
   in a form: a <= b <= d, a <= c, and if (a == b), c <= d. */ 
void SortSlaterKey(int *kd) {
  int i;

  if (kd[0] > kd[2]) {
    i = kd[0];
    kd[0] = kd[2];
    kd[2] = i;
  }

  if (kd[1] > kd[3]) {
    i = kd[1];
    kd[1] = kd[3];
    kd[3] = i;
  }
  
  if (kd[0] > kd[1]) {
    i = kd[0];
    kd[0] = kd[1];
    kd[1] = i;
    i = kd[2];
    kd[2] = kd[3];
    kd[3] = i;
  } else if (kd[0] == kd[1]) {
    if (kd[2] > kd[3]) {
      i = kd[2];
      kd[2] = kd[3];
      kd[3] = i;
    }
  }
}

int GetYk(int k, double *yk, ORBITAL *orb1, ORBITAL *orb2, int type) {
  int i, ilast;
  int i0;
  double a, max;

  for (i = 0; i < MAX_POINTS; i++) {
    _dwork1[i] = pow(potential->rad[i], k);
  }

  Integrate(_dwork1, orb1, orb2, type, _zk);
  
  for (i = 0; i < MAX_POINTS; i++) {
    _zk[i] /= _dwork1[i];
    yk[i] = _zk[i];
  }

  if (k > 2) {
    max = 0.0;
    for (i = 0; i < MAX_POINTS; i++) {
      a = fabs(_yk[i]);
      if (max < a) max = a;
    }
    max *= 1E-3;
    for (i = 0; i < Min(orb1->ilast, orb2->ilast); i++) {
      a = Large(orb1)[i]*Large(orb2)[i]*potential->rad[i];
      if (fabs(a) > max) break;
      _dwork1[i] = 0.0;
    }
    i0 = i;
  } else i0 = 0;
  for (i = i0; i < MAX_POINTS; i++) {
    _dwork1[i] = pow(potential->rad[i0]/potential->rad[i], k+1);
  }
  Integrate(_dwork1, orb1, orb2, type, _xk);
  ilast = MAX_POINTS - 1;
  for (i = i0; i < MAX_POINTS; i++) {
    _xk[i] = (_xk[ilast] - _xk[i])/_dwork1[i];
    yk[i] += _xk[i];
  }
  return 0;
}  

/* integrate a function given by f with two orbitals. */
/* type indicates the type of integral */
/* type = 1,    P1*P2 + Q1*Q2 */
/* type = 2,    P1*P1 */
/* type = 3,    Q1*Q1 */ 
/* type = 4:    P1*Q2 + Q1*P2 */
/* type = 5:    P1*Q2 - Q1*P2 */

int Integrate(double *f, ORBITAL *orb1, ORBITAL *orb2, 
	      int t, double *x) {
  int i1, i2;
  double a;
  int i;
  double *r;

  if (t == 0) t = 1;
  if (t < 0) r = x;
  else r = _dwork;
  r[0] = 0.0;

  if (orb1->n > 0 && orb2->n > 0) {
    i2 = Min(orb1->ilast, orb2->ilast);
    IntegrateSubRegion(0, i2, f, orb1, orb2, t, r, 0);
  } else if (orb1->n > 0 && orb2->n <= 0) {
    i1 = Min(orb1->ilast, orb2->ilast);
    IntegrateSubRegion(0, i1, f, orb1, orb2, t, r, 0);
    i1 += 1;
    i2 = orb1->ilast;
    IntegrateSubRegion(i1, i2, f, orb1, orb2, t, r, 1);
  } else if (orb1->n <= 0 && orb2->n > 0) {
    i1 = Min(orb1->ilast, orb2->ilast);
    IntegrateSubRegion(0, i1, f, orb1, orb2, t, r, 0);
    i1 += 1;
    i2 = orb2->ilast;
    IntegrateSubRegion(i1, i2, f, orb1, orb2, t, r, 2);
  } else {
    i1 = Min(orb1->ilast, orb2->ilast);
    IntegrateSubRegion(0, i1, f, orb1, orb2, t, r, 0);
    i1 += 1;
    if (i1 > orb1->ilast) {
      i2 = orb2->ilast;
      IntegrateSubRegion(i1, i2, f, orb1, orb2, t, r, 2);
      i1 = i2+1;
      i2 = MAX_POINTS-1;
      IntegrateSubRegion(i1, i2, f, orb1, orb2, t, r, 3);
    } else {
      i2 = orb1->ilast;
      IntegrateSubRegion(i1, i2, f, orb1, orb2, t, r, 1);
      i1 = i2+1;
      i2 = MAX_POINTS-1;
      IntegrateSubRegion(i1, i2, f, orb1, orb2, t, r, 3);
    }
  }

  if (t >= 0) {
    *x = r[i2-1];
  } else {
    i2++;
    for (i = i2+1; i < MAX_POINTS; i++) {
      r[i] = r[i2];
    }
  }
}

int IntegrateSubRegion(int i0, int i1, 
		       double *f, ORBITAL *orb1, ORBITAL *orb2,
		       int t, double *r, int m) {
  int i, j, ip, type;
  ORBITAL *tmp;
  double *large1, *large2, *small1, *small2;
  double *x, *y, *r1, *x1, *x2, *y1, *y2;
  double a, b;

  if (i1 <= i0) return 0;
  type = abs(t);

  x = _dwork3;
  y = _dwork4;
  x1 = _dwork5;
  x2 = _dwork6;
  y1 = _dwork7;
  y2 = _dwork8;
  r1 = _dwork9;

  switch (m) {
  case 0:  /* m = 0 */
    large1 = Large(orb1);
    large2 = Large(orb2);
    small1 = Small(orb1);
    small2 = Small(orb2);
    switch (type) {
    case 1: /* type = 1 */
      for (i = i0; i <= i1; i++) {
	x[i] = large1[i] * large2[i];
	x[i] += small1[i] * small2[i];
	x[i] *= f[i]*potential->dr_drho[i];
      }
      break;
    case 2: /* type = 2 */
      for (i = i0; i <= i1; i++) {
	x[i] = large1[i] * large2[i];
	x[i] *= f[i]*potential->dr_drho[i];
      }
      break;
    case 3: /*type = 3 */
      for (i = i0; i <= i1; i++) {
	x[i] = small1[i] * small2[i];
	x[i] *= f[i]*potential->dr_drho[i];
      }
      break;
    case 4: /*type = 4 */
      for (i = i0; i <= i1; i++) {
	x[i] = large1[i] * small2[i];
	x[i] += small1[i] * large2[i];
	x[i] *= f[i]*potential->dr_drho[i];
      }
      break;
    case 5: /* type = 5 */
      for (i = i0; i <= i1; i++) {
	x[i] = large1[i] * small2[i];
	x[i] -= small1[i] * large2[i];
	x[i] *= f[i]*potential->dr_drho[i];
      } 
      break;
    default: /* error */
      return -1;
    }
    NewtonCotes(r, x, i0, i1, t);
    break;

  case 1: 
  case 2: /* m = 1, 2 are essentially the same */
    if (m == 2) {
      tmp = orb1;
      orb1 = orb2;
      orb2 = tmp;
    }
    large1 = Large(orb1);
    large2 = Large(orb2);
    small1 = Small(orb1);
    small2 = Small(orb2);
    switch (type) {
    case 1: /* type = 1 */
      j = 0;
      for (i = i0; i <= i1; i+= 2) {
	ip = i+1;
	x[j] = large1[i] * large2[i];
	x[j] += small1[i] * small2[ip];
	x[j] *= f[i];
	y[j] = small1[i] * small2[i] * f[i];
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      if (i < MAX_POINTS) {
	ip = i+1;
	if (i > orb1->ilast) {
	  a = sin(large1[ip]);
	  b = cos(large1[ip]);
	  b = small1[i]*b + small1[ip]*a;
	  a = large1[i]*a;
	} else {
	  a = large1[i];
	  b = small1[i];
	}
	x[j] = a * large2[i];
	x[j] += b * small2[ip];
	x[j] *= f[i];
	y[j] = b * small2[i] * f[i];
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r, t);
      if (t < 0) {
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      }
      break;
    case 2: /* type = 2 */
      j = 0;
      for (i = i0; i <= i1; i+= 2) {
	ip = i+1;
	x[j] = large1[i] * large2[i];
	x[j] *= f[i]; 
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      if (i < MAX_POINTS) {
	ip = i+1;
	if (i > orb1->ilast) {
	  a = sin(large1[ip]);
	  a = large1[i]*a;
	} else {
	  a = large1[i];
	}
	x[j] = a * large2[i];
	x[j] *= f[i];
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, NULL, _phase, _dphase, i0, r, t);
      if (t < 0) {
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      }
      break;
    case 3: /* type = 3 */
      j = 0;
      for (i = i0; i <= i1; i+= 2) {
	ip = i+1;
	x[j] = small1[i] * small2[ip];
	x[j] *= f[i];
	y[j] = small1[i] * small2[i] * f[i];
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      if (i < MAX_POINTS) {
	ip = i+1;
	if (i > orb1->ilast) {
	  a = sin(large1[ip]);
	  b = cos(large1[ip]);
	  b = small1[i]*b + small1[ip]*a;
	} else {
	  b = small1[i];
	}
	x[j] += b * small2[ip];
	x[j] *= f[i];
	y[j] = b * small2[i] * f[i];
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r, t);
      if (t < 0) {
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      }
      break;  
    case 4: /* type = 4 */
      j = 0;
      for (i = i0; i <= i1; i+= 2) {
	ip = i+1;
	x[j] = large1[i] * small2[ip];
	x[j] += small1[i] * large2[i];
	x[j] *= f[i];
	y[j] = large1[i] * small2[i] * f[i];
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      if (i < MAX_POINTS) {
	ip = i+1;
	if (i > orb1->ilast) {
	  a = sin(large1[ip]);
	  b = cos(large1[ip]);
	  b = small1[i]*b + small1[ip]*a;
	  a = large1[i]*a;
	} else {
	  a = large1[i];
	  b = small1[i];
	}
	x[j] = a * small2[ip];
	x[j] += b * large2[i];
	x[j] *= f[i];
	y[j] = a * small2[i] * f[i];
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r, t);
      if (t < 0) {
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      }
      break;
    case 5: /* type = 5 */
      j = 0;
      for (i = i0; i <= i1; i+= 2) {
	ip = i+1;
	x[j] = large1[i] * small2[ip];
	x[j] -= small1[i] * large2[i];
	x[j] *= f[i];
	y[j] = -large1[i] * small2[i] * f[i];
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      if (i < MAX_POINTS) {
	ip = i+1;
	if (i > orb1->ilast) { 
	  a = sin(large1[ip]);
	  b = cos(large1[ip]);
	  b = small1[i]*b + small1[ip]*a;
	  a = large1[i]*a;
	} else {
	  a = large1[i];
	  b = small1[i];
	}
	x[j] = a * small2[ip];
	x[j] -= b * large2[i];
	x[j] *= f[i];
	y[j] = -a * small2[i] * f[i];
	_phase[j] = large2[ip];
	_dphase[j] = 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r, t);
      if (m == 2) {
	if (t < 0) {
	  for (i = i0; i <= i1; i += 2) {
	    r[i] = -r[i];
	  }
	  if (i < MAX_POINTS) r[i] = -r[i];
	} else {
	  i = i1 - 1;
	  r[i] = -r[i];
	  i = i1 + 1;
	  if (i < MAX_POINTS) r[i] = -r[i];
	}
      }
      if (t < 0) {
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      }
      break;
    default:
      return -1;
    }
    break;

  case 3: /* m = 3 */
    large1 = Large(orb1);
    large2 = Large(orb2);
    small1 = Small(orb1);
    small2 = Small(orb2);
    r1[i0] = 0.0;
    switch (type) {
    case 1: /* type = 1 */
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;	
	x1[j] = small1[i] * small2[ip];
	x2[j] = small1[ip] * small2[i];	
	x[j] = x1[j] + x2[j];
	x[j] *= 0.5*f[i];
	y1[j] = small1[i] * small2[i];
	y2[j] = small1[ip] * small2[ip];
	y2[j] += large1[i] * large2[i];
	y[j] = y1[j] - y2[j];
	y[j] *= 0.5*f[i];
	_phase[j] = large1[ip] + large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) + 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r, t);
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;
	x[j] = -x1[j] + x2[j];
	x[j] *= 0.5*f[i];
	y[j] = y1[j] + y2[j];
	y[j] *= 0.5*f[i];
	_phase[j] = large1[ip] - large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) - 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r1, t);
      if (t < 0) {
	for (i = i0; i <= i1; i += 2) {
	  r[i] += r1[i];
	}
	if (i < MAX_POINTS) r[i] += r1[i];
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      } else {
	i = i1-1;
	r[i] += r1[i];
      }
      break;	
    case 2: /* type = 2 */
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;
	y2[j] = -large1[i] * large2[i];
	y2[j] *= 0.5*f[i];
	y[j] = y2[j];
	_phase[j] = large1[ip] + large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) + 1.0/(large2[i]*large2[i]);
	j++;
      } 
      IntegrateSinCos(j, NULL, y, _phase, _dphase, i0, r, t);
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;
	y[j] = -y2[j];
	_phase[j] = large1[ip] - large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) - 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, NULL, y, _phase, _dphase, i0, r1, t);
      if (t < 0) {
	for (i = i0; i <= i1; i += 2) {
	  r[i] += r1[i];
	}
	if (i < MAX_POINTS) r[i] += r1[i];
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      } else {
	i = i1-1;
	r[i] += r1[i];
      }
      break;
    case 3: /* type = 3 */
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;
	x1[j] = small1[i] * small2[ip];
	x2[j] = small1[ip] * small2[i];
	x[j] = x1[j] + x2[j];
	x[j] *= 0.5*f[i];
	y1[j] = small1[i] * small2[i];
	y2[j] = small1[ip] * small2[ip];
	y[j] = y1[j] - y2[j];
	y[j] *= 0.5*f[i];
	_phase[j] = large1[ip] + large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) + 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r, t);
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;
	x[j] = -x1[j] + x2[j];
	x[j] *= 0.5*f[i];
	y[j] = y1[j] + y2[j];
	y[j] *= 0.5*f[i];
	_phase[j] = large1[ip] - large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) - 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r1, t);
      if (t < 0) {
	for (i = i0; i <= i1; i += 2) {
	  r[i] += r1[i];
	}
	if (i < MAX_POINTS) r[i] += r1[i];
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      } else {
	i = i1-1;
	r[i] += r1[i];
      }
      break;
    case 4: /* type = 4 */
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;
	x1[j] = large1[i] * small2[i];
	x2[j] = small1[i] * large2[i];
	x[j] = x1[j] + x2[j];
	x[j] *= 0.5*f[i];
	y[j] = -large1[i] * small2[ip];
	y[j] -= small1[ip] * large2[i];
	y[j] *= 0.5*f[i];
	y2[j] = y[j];
	_phase[j] = large1[ip] + large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) + 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r, t);
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;
	x[j] = x1[j] - x2[j];
	x[j] *= 0.5*f[i];
	y[j] = -y2[j];
	_phase[j] = large1[ip] - large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) - 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r1, t);
      if (t < 0) {
	for (i = i0; i <= i1; i += 2) {
	  r[i] += r1[i];
	}
	if (i < MAX_POINTS) r[i] += r1[i];
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      } else {
	i = i1-1;
	r[i] += r1[i];
      }
      break;
    case 5: /* type = 5 */
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;
	x1[j] = large1[i] * small2[i];
	x2[j] = small1[i] * large2[i];
	x[j] = x1[j] - x2[j];
	x[j] *= 0.5*f[i];
	y[j] = -large1[i] * small2[ip];
	y[j] += small1[ip] * large2[i];
	y[j] *= 0.5*f[i];
	y2[j] = y[j];
	_phase[j] = large1[ip] + large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) + 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r, t);
      j = 0;
      for (i = i0; i <= i1; i += 2) {
	ip = i+1;
	x[j] = x1[j] + x2[j];
	x[j] *= 0.5*f[i];
	y[j] = -y2[j];
	_phase[j] = large1[ip] - large2[ip];
	_dphase[j] = 1.0/(large1[i]*large1[i]) - 1.0/(large2[i]*large2[i]);
	j++;
      }
      IntegrateSinCos(j, x, y, _phase, _dphase, i0, r1, t);
      if (t < 0) {
	for (i = i0; i <= i1; i += 2) {
	  r[i] += r1[i];
	}
	if (i < MAX_POINTS) r[i] += r1[i];
	for (i = i0+1; i < i1; i += 2) {
	  r[i] = 0.5*(r[i-1] + r[i+1]);
	}
	if (i1 < MAX_POINTS-1) r[i1] = 0.5*(r[i1-1] + r[i1+1]);
	else r[i1] = r[i1-1];
      } else {
	i = i1-1;
	r[i] += r1[i];
      }
      break;
    default:
      return -1;
    }
  default:
    return -1; 
  }

  return 0;
}

int IntegrateSinCos(int j, double *x, double *y, 
		    double *phase, double *dphase, 
		    int i0, double *r, int t) {
  int i, k, m, n, q;
  double si0, si1, cs0, cs1, x0, x1, y0, y1;
  double is[4], ic[4], delta, p;
  double *z, *u, a[4], b[4], h, dr;

  z = _dwork10;
  u = _dwork11;
  
  for (i = 1, k = i0+2; i < j; i++, k += 2) {
    h = dphase[i-1]+dphase[i];
    dr = potential->rad[k] - potential->rad[k-2];
    if (h*dr > 0.1) break;
    z[i] = 0.0;
    if (x) z[i] += x[i]*sin(phase[i]);
    if (y) z[i] += y[i]*cos(phase[i]);
    z[i] *= potential->dr_drho[k];
  }
  if (i > 1) {
    z[0] = 0.0;
    if (x) z[0] += x[0]*sin(phase[0]);
    if (y) z[0] += y[0]*cos(phase[0]);
    z[0] *= potential->dr_drho[i0];
    u[0] = 0.0;
    NewtonCotes(u, z, 0, i-1, t);
    for (m = 1, n = i0+2; m < i; m++, n += 2) {
      r[n] = r[i0] + 2.0*u[m];
    }
  }  
  m = j-i+1;
  if (m < 2) {
    r[k] = r[k-2];
    return 0;
  }
  if (i > 1) {
    q = i-2;
    m++;
  } else {
    q = i-1;
  }
  if (x) {
    for (n = q; n < j; n++) {
      x[n] /= dphase[n];
    }
    spline(phase+q, x+q, m, 1E30, 1E30, z+q);
  } 
  if (y) {
    for (n = q; n < j; n++) {
      y[n] /= dphase[n];
    }
    spline(phase+q, y+q, m, 1E30, 1E30, u+q);
  } 

  si0 = sin(phase[i-1]);
  cs0 = cos(phase[i-1]);
  for (; i < j; i++, k += 2) {
    delta = phase[i] - phase[i-1];
    si1 = sin(phase[i]);
    cs1 = cos(phase[i]);
    is[0] = -(cs1 - cs0);
    ic[0] = si1 - si0;
    p = delta;
    for (m = 1; m < 4; m++) {
      is[m] = -p * cs1 + m*ic[m-1];
      ic[m] =  p * si1 - m*is[m-1];
      p *= delta;
    }
    r[k] = r[k-2];
    if (x) {
      a[3] = (z[i] - z[i-1])/(6.0*delta);
      a[2] = z[i-1]/3.0;
      a[1] = (x[i] - x[i-1])/delta - (z[i] + z[i-1])*delta/6.0;
      a[0] = x[i-1];
      r[k] += a[0]*is[0] 
	+ a[1]*is[1]
	+ a[2]*is[2]
	+ a[3]*is[3];
    } 
    if (y) {
      b[3] = (u[i] - u[i-1])/(6.0*delta);
      b[2] = u[i-1]/3.0;
      b[1] = (y[i] - y[i-1])/delta - (u[i] + u[i-1])*delta/6.0;
      b[0] = y[i-1];
      r[k] += b[0]*ic[0] 
	+ b[1]*ic[1]
	+ b[2]*ic[2]
	+ b[3]*ic[3]; 
    }
    si0 = si1;
    cs0 = cs1;
  }
  
  return 0;
}

int FreeSlaterArray() {
  if (slater_array->array == NULL) return 0;
  MultiFreeData(slater_array->array, slater_array->ndim, NULL);
  return 0;
}

int FreeResidualArray() {
  if (residual_array->array == NULL) return 0;
  MultiFreeData(residual_array->array, residual_array->ndim, NULL);
  return 0;
}

int FreeMultipoleArray() {
  if (slater_array->array == NULL) return 0;
  MultiFreeData(multipole_array->array, multipole_array->ndim, NULL);
  return 0;
}


int InitRadial() {
  int i;
  int ndim;
  int blocks[6] = {5, 5, 5, 5, 5, 5};

  potential = malloc(sizeof(POTENTIAL));
  potential->flag = 0;
  n_orbitals = 0;
  n_continua = 0;
  
  orbitals = malloc(sizeof(ARRAY));
  if (!orbitals) return -1;
  if (ArrayInit(orbitals, sizeof(ORBITAL), ORBITALS_BLOCK) < 0) return -1;

  ndim = 6;
  slater_array = (MULTI *) malloc(sizeof(MULTI));
  MultiInit(slater_array, sizeof(double), ndim, blocks);
  
  ndim = 2;
  residual_array = (MULTI *) malloc(sizeof(MULTI));
  MultiInit(residual_array, sizeof(double), ndim, blocks);
  
  ndim = 4;
  blocks[0] = 10;
  multipole_array = (MULTI *) malloc(sizeof(MULTI));
  MultiInit(multipole_array, sizeof(double), ndim, blocks);

  return 0;
}

int TestIntegrate(char *s) {
  ORBITAL *orb1, *orb2, *orb3, *orb4;
  int k1, k2, k3, k4, i;
  double r;
  FILE *f;
 
  k1 = OrbitalIndex(3, -1, 0.0);
  k2 = OrbitalIndex(0, 1, 5000.0/HARTREE_EV);
  orb1 = GetOrbital(k1);
  orb2 = GetOrbital(k2);
  for (i = 0; i < MAX_POINTS; i++) { 
    _yk[i] = potential->rad[i]; 
  } 
  Integrate(_yk, orb1, orb2, -1, _xk);  
  f = fopen(s, "w");
  fprintf(f, "# %d %d %10.3E %10.3E\n", 
	  orb1->ilast, orb2->ilast, 
	  orb1->energy, orb2->energy);
  for (i = 0; i < MAX_POINTS; i++) {
    fprintf(f, "%d %10.3E %10.3E %10.3E %10.3E %10.3E %10.3E %10.3E\n", 
	    i, potential->rad[i],  
	    _xk[i], potential->Vc[i],
	    potential->U[i], potential->Vtail[i], 
	    Large(orb1)[i], Large(orb2)[i]);
  }

  /*
  GetYk(1, _yk, orb1, orb2, -1); 

  for (i = 0; i < MAX_POINTS; i++) { 
    _yk[i] /= potential->rad[i]; 
  } 
  k3 = OrbitalIndex(0, 17, (5000.0+50.0)/HARTREE_EV);   
  k4 = OrbitalIndex(0, -17, 5000.0/HARTREE_EV);    
  orb3 = GetOrbital(k3);  
  orb4 = GetOrbital(k4);  
 
  Integrate(_yk, orb3, orb4, -1, _xk); 
  
  f = fopen(s, "w");
  fprintf(f, "# %d %d %d %d %10.3E %10.3E %10.3E %10.3E\n", 
	  orb1->ilast, orb2->ilast, orb3->ilast, orb4->ilast, 
	  orb1->energy, orb2->energy,
	  orb3->energy, orb4->energy);
  for (i = 0; i < MAX_POINTS; i++) {
    fprintf(f, "%d %10.3E %10.3E %10.3E %10.3E %10.3E %10.3E %10.3E %10.3E %10.3E %10.3E %10.3E\n", 
	    i, potential->rad[i],  
	    _yk[i], _zk[i], _xk[i], potential->Vc[i],
	    potential->U[i], potential->Vtail[i],
	    Large(orb1)[i], Large(orb2)[i], 
	    Large(orb3)[i], Large(orb4)[i]);
  }
  */
  fclose(f); 
}

  

