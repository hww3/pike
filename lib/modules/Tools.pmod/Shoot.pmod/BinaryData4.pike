#pike __REAL_VERSION__
inherit Tools.Shoot.Test;

constant name="Read binary INT16";

int k = 100;
int m = 10000;
int n = m*k; // for reporting

void perform()
{
   for (int i=0; i<k; i++)
      array_sscanf(random_string(2*m),"%2c"*m);
}

string present_n(int ntot,int nruns,float tseconds,float useconds,int memusage)
{
   return sprintf("%.0f/s",ntot/useconds);
}
