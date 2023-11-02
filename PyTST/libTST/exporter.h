#include "callbacks.h"
#include "dish.h"
#include <array>
#include <iostream>
#include <pybind11/numpy.h>
#include "conrec.h"

const int colortable[256][3] = {
#include "default.txt"
};

void Dish::Init()
{
    CPM->GrowInCells(par.n_init_cells, par.size_init_cells, par.subfield);
    CPM->ConstructInitCells(*this);
    for (int i = 0; i < par.divisions; i++)
        CPM->DivideCells();

    CPM->SetRandomTypes();
    std::cout << "Setting init callback" << std::endl;
    CPM->getCallback()->callback_init({ par.sizey, par.sizex, CPM->GetSigmaPtr()[0] });
    std::cout << "Set init callback" << std::endl;

    setTargetArea();
}

int PDE::MapColour(double val)
{
    return 0;
}

class Exporter {
private:
    Dish* dish;

public:
    Exporter(const std::string&);
    ~Exporter();
    void timestep();
    int getSigmaAt(const int, const int);
    cbSigma getSigma();
    void setCallback(Callback*);
    void PlotPDE(pybind11::array_t<int> drawing, int l, int color) ;
    int getNumOfCells() { return dish->CountCells(); }
    int getColorOfCell(int cell)
    {
        return dish->CPM->getCell(cell).Colour();
    }
    void dishInit()
    {
        dish->Init();
    }
    int copy_prob(double DH, double stiff)
    {
        return dish->CPM->CopyvProb(DH, stiff);
    }
    cbPDESigma getPDE(int layer);
    void computeDrawing(pybind11::array_t<int> drawing, int border_color);
    // pybind11::array_t<int> computeDrawing(pybind11::array_t<int> drawing, int border_color);
    //
    void SecreteAndDiffuse();
};

Exporter::Exporter(const std::string& filename)
{
    std::cout << "Running exporter constructor" << std::endl;
    par.Read(filename.c_str());
    std::cout << "Done reading.." << std::endl;
    Seed(par.rseed);
    dish = new Dish;
    std::cout << "Done with: Running exporter constructor" << std::endl;
};
Exporter::~Exporter()
{
    std::cout << "Exporter is getting deleted!!" << std::endl;
    delete dish;
}
void Exporter::timestep()
{
    static int i = 0;
    dish->CPM->AmoebaeMove(dish->PDEfield);
    dish->CPM->getCallback()->callback_timestep({ par.sizey, par.sizex, dish->CPM->GetSigmaPtr()[0] });
    i++;
}
int Exporter::getSigmaAt(const int x, const int y)
{
    return dish->CPM->Sigma(x, y);
}
cbSigma Exporter::getSigma()
{
    int** p = dish->CPM->GetSigmaPtr();
    int* q = p[0];
    return cbSigma(par.sizey, par.sizex, q);
}

cbPDESigma Exporter::getPDE(int layer)
{
    double** p = dish->PDEfield->SigmaPointer(layer);
    double* q = p[0];
    return cbPDESigma(par.sizey, par.sizex, q);
}

void Exporter::setCallback(Callback* callback)
{
    dish->CPM->setCallback(callback);
}

void setValue(int i, int j, int k, int value, int* data, int shape1, int shape2)
{
    // used in computeDrawing
    int index = i * shape2 * shape1 + shape2 * j + k;
    data[index] = value;
}

// pybind11::array_t<int> Exporter::computeDrawing(pybind11::array_t<int> drawing, int border_color)
void Exporter::computeDrawing(pybind11::array_t<int> drawing, int border_color)
{
    pybind11::buffer_info drawing_info = drawing.request();
    int* drawing_ptr = static_cast<int*>(drawing_info.ptr);

    for (int i = 1; i < par.sizex - 1; i++) {
        for (int j = 1; j < par.sizey - 1; j++) {

            int spin = getSigmaAt(i, j);
            int type = dish->CPM->getCell(spin).getTau();

            if (spin >0)
                type++;

            int R = colortable[type][0];
            int G = colortable[type][1];
            int B = colortable[type][2];

            int R_border = colortable[border_color][0];
            int G_border = colortable[border_color][1];
            int B_border = colortable[border_color][2];

            setValue(2 * i, 2 * j, 0, R, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
            setValue(2 * i, 2 * j, 1, G, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
            setValue(2 * i, 2 * j, 2, B, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);

            if (spin == getSigmaAt(i + 1, j)) {
                setValue(2 * i + 1, 2 * j, 0, R, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i + 1, 2 * j, 1, G, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i + 1, 2 * j, 2, B, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
            } else {
                setValue(2 * i + 1, 2 * j, 0, R_border, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i + 1, 2 * j, 1, G_border, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i + 1, 2 * j, 2, B_border, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
            }

            if (spin == getSigmaAt(i, j + 1)) {
                setValue(2 * i, 2 * j + 1, 0, R, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i, 2 * j + 1, 1, G, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i, 2 * j + 1, 2, B, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
            } else {
                setValue(2 * i, 2 * j + 1, 0, R_border, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i, 2 * j + 1, 1, G_border, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i, 2 * j + 1, 2, B_border, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
            }

            if (spin == getSigmaAt(i + 1, j + 1)) {
                setValue(2 * i + 1, 2 * j + 1, 0, R, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i + 1, 2 * j + 1, 1, G, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i + 1, 2 * j + 1, 2, B, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
            } else {
                setValue(2 * i + 1, 2 * j + 1, 0, R_border, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i + 1, 2 * j + 1, 1, G_border, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
                setValue(2 * i + 1, 2 * j + 1, 2, B_border, drawing_ptr, drawing_info.shape[1], drawing_info.shape[2]);
            }
        }
    }
}

void Exporter::SecreteAndDiffuse() {
	dish->PDEfield->Secrete(dish->CPM);
	dish->PDEfield->Diffuse(1);
}


void PDE::Secrete(CellularPotts *cpm) {

  const double dt=par.dt;

  for (int x=0;x<sizex;x++)
    for (int y=0;y<sizey;y++) {
      // inside cells
      if (cpm->Sigma(x,y)) {
	
	sigma[0][x][y]+=par.secr_rate[0]*dt;
	
      } else {
      // outside cells
	sigma[0][x][y]-=par.decay_rate[0]*dt*sigma[0][x][y];
 
      }
    }
}


void Exporter::PlotPDE(pybind11::array_t<int> drawing, int l, int color) {
    int nc = 10;
    double **sigma = dish->PDEfield->SigmaPointer(l) ;
  double *z=(double *)malloc(nc*sizeof(double));
  double max = dish->PDEfield->Max(l);
  double min = dish->PDEfield->Min(l);
  double step=(max-min)/nc;
  {for (int i=0;i<nc;i++)
    z[i]=(i+1)*step;}
  
  double *x=(double *)malloc(par.sizex*sizeof(double));
  {for (int i=0;i<par.sizex;i++)
    x[i]=i;}
  
  double *y=(double *)malloc(par.sizey*sizeof(double));
  {for (int i=0;i<par.sizey;i++)
    y[i]=i;}

    conrec(sigma,0, par.sizex-1,0,par.sizey-1,x,y,nc,z,drawing,color);

  
  free(x);
  free(y);
  free(z);
}
