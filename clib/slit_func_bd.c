#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

int bandsol(int n, int nd, double (*a)[n], double *r)
{
  double aa;
  int i, j, k;

  /*
   bandsol solve a sparse system of linear equations with band-diagonal matrix.
   Band is assumed to be symmetrix relative to the main diaginal.
   Parameters are:
         a is 2D array [n,nd] where n - is the number of equations and nd
           is the width of the band (3 for tri-diagonal system).
           nd must be an odd number. The main diagonal should be in a(*,nd/2)
           The first lower subdiagonal should be in a(1:n-1,nd/2-1), the first
           upper subdiagonal is in a(0:n-2,nd/2+1) etc. For example:
                  / 0 0 X X X \
                  | 0 X X X X |
                  | X X X X X |
                  | X X X X X |
              A = | X X X X X |
                  | X X X X X |
                  | X X X X X |
                  | X X X X 0 |
                  \ X X X 0 0 /
         r is the array of RHS of size n.
   bandsol returns 0 on success, -1 on incorrect size of "a" and -4 on degenerate
   matrix.
*/

  //  if(nd % 2 ==0) return -1;

  /* Forward sweep */
  for (i = 0; i < n - 1; i++)
  {
    aa = a[nd / 2][i];
    //if(aa==0.e0) return -3;
    r[i] /= aa;
    for (j = 0; j < nd; j++)
      a[j][i] /= aa;
    for (j = 1; j < min(nd / 2 + 1, n - i); j++)
    {
      aa = a[nd / 2 - j][i + j];
      //if(aa==0.e0) return -j;
      r[i + j] -= r[i] * aa;
      for (k = 0; k < (nd - j); k++)
        a[k][i + j] -= a[k + j][i] * aa;
    }
  }

  /* Backward sweep */
  r[n - 1] /= a[nd / 2][n - 1];
  for (i = n - 1; i > 0; i--)
  {
    for (j = 1; j <= min(nd / 2, i); j++)
      r[i - j] -= r[i] * a[nd / 2 + j][i - j];
    r[i - 1] /= a[nd / 2][i - 1];
  }

  r[0] /= a[nd / 2][0];

  return 0;
}

int slit_func_vert(int ncols,        /* Swath width in pixels                                 */
                   int nrows,        /* Extraction slit height in pixels                      */
                   double *im,       /* Image to be decomposed                                */
                   int *mask,        /* Initial and final mask for the swath                  */
                   double *ycen,     /* Order centre line offset from pixel row boundary      */
                   int osample,      /* Subpixel ovsersampling factor                         */
                   double lambda_sP, /* Smoothing parameter for the spectrum, coiuld be zero  */
                   double lambda_sL, /* Smoothing parameter for the slit function, usually >0 */
                   double *sP,       /* Spectrum resulting from decomposition                 */
                   double *sL,       /* Slit function resulting from decomposition            */
                   double *model,    /* Model constructed from sp and sf                      */
                   double *unc       /* Spectrum uncertainties                                */
)
{
  int x, y, iy, jy, ny, iy1, iy2;
  double step, d1, d2, sum, norm, dev, lambda, diag_tot, sP_change, sP_max;
  int info, iter, isum, nd;

  nd = 2 * osample + 1;
  ny = osample * (nrows + 1) + 1; /* The size of the sf array */
  step = 1.e0 / osample;

  double *E = malloc(ncols * sizeof(double));                 // double E[ncols];
  double *sP_old = malloc(ncols * sizeof(double));            // double sP_old[ncols];
  double(*Aij)[ny] = malloc(nd * sizeof(*Aij));               // double Aij[nd][ny];
  double *bj = malloc(ny * sizeof(double));                   // double bj[ny];
  double(*Adiag)[ncols] = malloc(3 * sizeof(*Adiag));         // double Adiag[3][ncols];
  double(*omega)[nrows][ncols] = malloc(ny * sizeof(*omega)); // double omega[ny][nrows][ncols];

  /*
  Construct the omega tensor. Normally it has the dimensionality of ny*nrows*ncols. 
  The tensor is mostly empty and can be easily compressed to ny*nx, but this will
  complicate matrix operations at later stages. I will keep it as it is for now.
  Note, that omega is used in in the equations for sL, sP and for the model but it
  does not involve the data, only the geometry. Thus it can be pre-computed once.
  */

  for (x = 0; x < ncols; x++)
  {
    iy2 = osample - floor(ycen[x] / step) - 1;
    iy1 = iy2 - osample;

    d1 = fmod(ycen[x], step);
    if (d1 == 0)
      d1 = step;
    d2 = step - d1;

    for (y = 0; y < nrows; y++)
    {
      iy1 += osample;
      iy2 += osample;
      for (iy = 0; iy < ny; iy++)
      {
        if (iy < iy1)
          omega[iy][y][x] = 0.;
        else if (iy == iy1)
          omega[iy][y][x] = d1;
        else if ((iy > iy1) && (iy < iy2))
          omega[iy][y][x] = step;
        else if (iy == iy2)
          omega[iy][y][x] = d2;
        else
          omega[iy][y][x] = 0.;
      }
    }
  }

  /* Loop through sL , sP reconstruction until convergence is reached */
  iter = 0;
  do
  {
    /*  Compute slit function sL */
    /* Fill in band-diagonal SLE array and the RHS */

    diag_tot = 0.e0;
    for (iy = 0; iy < ny; iy++)
    {
      bj[iy] = 0.e0;
      for (jy = max(iy - osample, 0); jy <= min(iy + osample, ny - 1); jy++)
      {
        Aij[jy - iy + osample][iy] = 0.e0;
        for (x = 0; x < ncols; x++)
        {
          sum = 0.e0;
          for (y = 0; y < nrows; y++)
            sum += omega[iy][y][x] * omega[jy][y][x] * mask[y * ncols + x];
          Aij[jy - iy + osample][iy] += sum * sP[x] * sP[x];
        }
      }
      for (x = 0; x < ncols; x++)
      {
        sum = 0.e0;
        for (y = 0; y < nrows; y++)
          sum += omega[iy][y][x] * mask[y * ncols + x] * im[y * ncols + x];
        bj[iy] += sum * sP[x];
      }
      diag_tot += Aij[osample][iy];
    }
    /* Scale regularization parameters */

    lambda = lambda_sL * diag_tot / ny;

    /* Add regularization parts for the SLE matrix */

    Aij[nd / 2][0] += lambda;     /* Main diagonal  */
    Aij[nd / 2 + 1][0] -= lambda; /* Upper diagonal */
    for (iy = 1; iy < ny - 1; iy++)
    {
      Aij[nd / 2 - 1][iy] -= lambda;    /* Lower diagonal */
      Aij[nd / 2][iy] += lambda * 2.e0; /* Main diagonal  */
      Aij[nd / 2 + 1][iy] -= lambda;    /* Upper diagonal */
    }
    Aij[nd / 2 - 1][ny - 1] -= lambda; /* Lower diagonal */
    Aij[nd / 2][ny - 1] += lambda;     /* Main diagonal  */

    /* Solve the system of equations */
    info = bandsol(ny, nd, Aij, bj);
    if (info)
      printf("info(sL)=%d\n", info);

    /* Normalize the slit function */

    norm = 0.e0;
    for (iy = 0; iy < ny; iy++)
    {
      sL[iy] = bj[iy];
      norm += sL[iy];
    }
    norm /= osample;
    for (iy = 0; iy < ny; iy++)
      sL[iy] /= norm;

    /*  Compute spectrum sP */
    for (x = 0; x < ncols; x++)
    {
      Adiag[1][x] = 0.e0;
      E[x] = 0.e0;
      for (y = 0; y < nrows; y++)
      {
        sum = 0.e0;
        for (iy = 0; iy < ny; iy++)
        {
          sum += omega[iy][y][x] * sL[iy];
        }
        Adiag[1][x] += sum * sum * mask[y * ncols + x];
        E[x] += sum * im[y * ncols + x] * mask[y * ncols + x];
      }
    }

    if (lambda_sP > 0.e0)
    {
      norm = 0.e0;
      for (x = 0; x < ncols; x++)
      {
        sP_old[x] = sP[x];
        norm += sP[x];
      }
      norm /= ncols;
      lambda = lambda_sP * norm;
      Adiag[0][0] = 0.e0;    // 0,0
      Adiag[1][0] += lambda; // 1, 0
      Adiag[2][0] -= lambda; // 2, 0
      for (x = 1; x < ncols - 1; x++)
      {
        Adiag[0][x] -= lambda;
        Adiag[1][x] += 2.e0 * lambda;
        Adiag[2][x] -= lambda;
      }
      Adiag[0][ncols - 1] -= lambda; //0, -1
      Adiag[1][ncols - 1] += lambda; // 1, -1
      Adiag[2][ncols - 1] = 0.e0;    // 2, -1

      info = bandsol(ncols, 3, Adiag, E);
      if (info)
        printf("info = %d\n", info);

      for (x = 0; x < ncols; x++)
      {
        sP[x] = E[x];
      }
    }
    else
    {
      for (x = 0; x < ncols; x++)
      {
        sP_old[x] = sP[x];
        sP[x] = E[x] / Adiag[1][x];
      }
    }

    /* Compute the model */

    for (y = 0; y < nrows; y++)
    {
      for (x = 0; x < ncols; x++)
      {
        sum = 0.e0;
        for (iy = 0; iy < ny; iy++)
          sum += omega[iy][y][x] * sL[iy];
        model[y * ncols + x] = sum * sP[x];
      }
    }

    /* Compare model and data */

    sum = 0.e0;
    isum = 0;
    for (y = 0; y < nrows; y++)
    {
      for (x = 0; x < ncols; x++)
      {
        sum += mask[y * ncols + x] * (model[y * ncols + x] - im[y * ncols + x]) * (model[y * ncols + x] - im[y * ncols + x]);
        isum += mask[y * ncols + x];
      }
    }
    dev = sqrt(sum / isum);

    /* Adjust the mask marking outlyers */

    for (y = 0; y < nrows; y++)
    {
      for (x = 0; x < ncols; x++)
      {
        if (fabs(model[y * ncols + x] - im[y * ncols + x]) > 6. * dev)
          mask[y * ncols + x] = 0;
        else
          mask[y * ncols + x] = 1;
      }
    }
    // printf("iter=%2i,\tdev=%.5g,\tsum=%.5g\n", iter, dev, sum);

    /* Compute the change in the spectrum */

    sP_change = 0.e0;
    sP_max = 1.e0;
    for (x = 0; x < ncols; x++)
    {
      if (sP[x] > sP_max)
        sP_max = sP[x];
      if (fabs(sP[x] - sP_old[x]) > sP_change)
        sP_change = fabs(sP[x] - sP_old[x]);
    }

    /* Check the convergence */

  } while (iter++ < 20 && sP_change > 1.e-5 * sP_max);

  for (x = 0; x < ncols; x++)
  {
    unc[x] = 0.;
    norm = 0.;
    for (y = 0; y < nrows; y++)
    {
      norm += mask[(y * ncols) + x];
      unc[x] += (model[(y * ncols) + x] - im[(y * ncols) + x]) * (model[(y * ncols) + x] - im[(y * ncols) + x]) *
                mask[(y * ncols) + x];
    }
    unc[x] = sqrt(unc[x] / norm * nrows);
  }

  free(E);
  free(sP_old);
  free(omega);
  free(Aij);
  free(bj);
  free(Adiag);

  return 0;
}
