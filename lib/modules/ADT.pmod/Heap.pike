#define SWAP(X,Y) do{ mixed tmp=values[X]; values[X]=values[Y]; values[Y]=tmp; }while(0)

static private array values=allocate(10);
static private int num_values;

#ifdef DEBUG
void verify_heap()
{
  int e;
  for(e=1;e<num_values;e++)
  {
    if(values[(e-1)/2] > values[e])
    {
      write(sprintf("FEL in HEAP (%d, %d) num_values=%d\n", (e-1)/2, e, num_values));
      exit(1);
    }
  }
}
#else
#define verify_heap()
#endif

static void adjust_down(int elem)
{
  int steps;
  while(1)
  {
    int child=elem*2+1;
    if(child >= num_values) break;
    
    if(child+1==num_values || values[child] < values[child+1])
    {
      if(values[child] < values[elem])
      {
	SWAP(child, elem);
	elem=child;
	continue;
      }
    } else {
      if(child+1 >= num_values) break;
      
      if(values[child+1] < values[elem])
      {
	SWAP(elem, child+1);
	elem=child+1;
	continue;
      }
    }
    break;
  }
}

static int adjust_up(int elem)
{
  int parent=(elem-1)/2;

  if(!elem) return 0;
  
  if(values[elem] < values[parent])
  {
    SWAP(elem, parent);
    elem=parent;
    while(values[elem] < values[parent=(elem -1)/2])
    {
      SWAP(elem, parent);
      elem=parent;
    }
    adjust_down(elem);
    return 1;
  }
  return 0;
}

void push(mixed value)
{
  if(num_values >= sizeof(values))
    values+=allocate(10+sizeof(values)/4);
  
  values[num_values++]=value;
  adjust_up(num_values-1);
  verify_heap();
}

void adjust(mixed value)
{
  int pos=search(values, value);
  if(pos>=0)
    if(!adjust_up(pos))
      adjust_down(pos);
  verify_heap();
}

mixed top()
{
  mixed ret;
  if(!num_values)
    error("Heap underflow!\n");
  
  ret=values[0];
  if(sizeof(values) > 1)
  {
    num_values--;
    values[0]=values[num_values];
    values[num_values]=0;
    adjust_down(0);
    
    if(num_values * 3 + 10 < sizeof(values))
      values=values[..num_values+10];
  }
  return ret;
  verify_heap();
}

int size() { return num_values; }

