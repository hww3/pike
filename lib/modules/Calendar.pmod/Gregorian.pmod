// by Mirar 

//!   
//! submodule Gregorian
//!	time units:
//!	<ref>Year</ref>, <ref>Month</ref>, <ref>Week</ref>, <ref>Day</ref>
//!
//! class 

array(string) month_names=
   ({"January","February","Mars","April","May","June","July","August",
     "September","October","November","December"});

array(string) week_day_names=
   ({"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"});

/* calculated on need */

mapping week_day_mapping,month_mapping;

class _TimeUnit
{
   inherit Calendar._TimeUnit;

   program vYear=function_object(object_program(this_object()))->Year;
   program vDay=function_object(object_program(this_object()))->Day;
   program vMonth=function_object(object_program(this_object()))->Month;
   program vWeek=function_object(object_program(this_object()))->Week;
   program vHour=function_object(object_program(this_object()))->Hour;
   program vMinute=function_object(object_program(this_object()))->Minute;
   program vSecond=function_object(object_program(this_object()))->Second;
}

//== Year ====================================================================

class Year
{
//! class Year
//! 	A <ref>Calendar.time_unit</ref>. 
//!
//!	Lesser units: <ref>Month</ref>, <ref>Week</ref>, <ref>Day</ref>
//!	Greater units: none
//!
//!     
   inherit _TimeUnit;

//-- variables ------------------------------------------------------

   int y;

   array days_per_month;
   array month_start_day;

//-- standard methods -----------------------------------------------

   array(string) lesser() 
   { 
      return ({"month","week","day"});
   }

   array(string) greater()
   {
      return ({});
   }

   void create(int ... arg)
   {
      if (!sizeof(arg))
      {
	 object yp=vDay()->year();
	 y=yp->y;
      }
      else
	 y=arg[0];

      int l=this->leap();
      days_per_month=
	 ({-17, 31,28+l,31,30,31,30,31,31,30,31,30,31});

      month_start_day=allocate(sizeof(days_per_month));

      for (int i=1,l=0; i<sizeof(days_per_month); i++)
	 month_start_day[i]=l,l+=days_per_month[i];
   }

   int `<(object x)
   {
      return y<(int)x;
   }

   int `==(object x)
   {
      return 
	 object_program(x)==object_program(this) &&
	 (int)x==y;
   }

   int `>(object x)
   {
      return y>(int)x;
   }

   object next()
   {
      return vYear(y+1);
   }

   object prev()
   {
      return vYear(y-1);
   }

   object `+(int n)
   {
      return vYear(y+n);
   }

   int|object `-(int|object n)
   {
      if (objectp(n) && object_program(n)==vYear)  return y-n->y;
      return this+-n;
   }

//-- nonstandard methods --------------------------------------------

   int julian_day(int d) // jd%7 gives weekday, mon=0, sun=6
   {
      int a;
      a = (y-1)/100;
      return 1721426 + d - a + (a/4) + (36525*(y-1))/100;
   }

   int leap()
   {
      if (!(y%400)) return 1;
      if (!(y%100)) return 0;
      return !(y%4);
   }

   int leap_day()
   {
      return 31+24-1; // 24 Feb
   }

   int number_of_weeks()
   {
      int j=julian_day(0)%7;

      // years that starts on a thursday has 53 weeks
      if (j==3) return 53; 
      // leap years that starts on a wednesday has 53 weeks
      if (j==2 && this->leap()) return 53;
      // other years has 52 weeks
      return 52;
   }

   int number_of_days()
   {
      return 365+this->leap();
   }

   int number()
   {
      return y;
   }

   string name()
   {
      if (y>0) return (string)y+" AD";
      else return (string)(-y+1)+" BC";
   }

   mixed cast(string what)
   {
      switch (what)
      {
	 case "int": return this->number(); 
	 case "string": return this->name();
	 default:
	    throw(({"can't cast to "+what+"\n",backtrace()}));
      }
   }

//-- less -----------------------------------------------------------

   object month(object|string|int n)
   {
      if (objectp(n))
	 if (object_program(n)==vMonth)
	    n=n->number();

      if (stringp(n))
      {
	 if (!month_mapping)
	 {
	    month_mapping=
	       mkmapping(Array.map(month_names,lower_case),
			 indices(allocate(13))[1..]);
	 }
	 n=month_mapping[lower_case(n)];
	 if (!n) return 0;
      }

      if (n<0)
	 return vMonth(y,13+n);
      else
	 return vMonth(y,n||1);
   }

   array(mixed) months()
   {
      return month_names;
   }

   object week(object|int n)
   {
      if (objectp(n))
	 if (object_program(n)==vWeek)
	 {
	    n=n->number();
	    if (n>number_of_weeks()) return 0; /* no such week */
	 }

      if (n<0)
	 return vWeek(y,this->number_of_weeks()+n+1);
      else
	 return vWeek(y,n||1);
   }

   array(mixed) weeks()
   {
       return indices(allocate(this->number_of_weeks()+1))[1..];
   }

   object day(object|int n)
   {
      if (objectp(n))
	 if (object_program(n)==vDay)
	 {
	    object o=n->year();
	    n=n->year_day();
	    if (o->leap() && n==o->leap_day())
	       if (!this->leap()) return 0;
	       else n=this->leap_day();
	    else
	    {
	       if (o->leap() && n>o->leap_day()) n--;
	       if (this->leap() && n>=this->leap_day()) n++;
	    }
	    if (n>=number_of_days()) return 0; /* no such day */
	 }
	 else return 0; /* illegal object */

      if (n<0)
	 return vDay(y,this->number_of_days()+n);
      else
	 return vDay(y,n);
   }

   array(mixed) days()
   {
       return indices(allocate(this->number_of_days()));
   }

//-- more -----------------------------------------------------------
     
   // none
};


//

//== Month ===================================================================

class Month
{
   inherit _TimeUnit;
//-- variables ------------------------------------------------------

   int y;
   int m;

//-- standard methods -----------------------------------------------

   array(string) lesser() 
   { 
      return ({"day"});
   }

   array(string) greater()
   {
      return ({"year"});
   }

   void create(int ... arg)
   {
      if (!sizeof(arg))
      {
	 object mp=vDay()->month();
	 y=mp->y;
	 m=mp->m;
      }
      else
      {
	 y=arg[0];
	 m=arg[1];
      }
   }
      
   int `<(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->y==y && x->m<m) || (x->y<y));
   }

   int `==(object x)
   {
      return 
	 objectp(x) &&
	 object_program(x)==object_program(this) &&
	 x->y==y && x->m==m;
   }

   int `>(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->y==y && x->m>m) || (x->y>y));
   }

   object `+(int n)
   {
      int m2=m;
      int y2=y;
      m2+=n;
      while (m2>12) { y2++; m2-=12; }
      while (m2<1) { y2--; m2+=12; }
      return vMonth(y2,m2);
   }

   int|object `-(object|int n)
   {
      if (objectp(n) && object_program(n)==vMonth) 
	 return m-n->m+(y-n->y)*12;
      return this+(-n);
   }

//-- internal -------------------------------------------------------

   int yday()
   {
      return this->year()->month_start_day[m];
   }

//-- nonstandard methods --------------------------------------------

   int number_of_days()
   {
      return this->year()->days_per_month[m];
   }

   int number()
   {
      return m;
   }

   string name()
   {
      return month_names[this->number()-1];
   }

   mixed cast(string what)
   {
      switch (what)
      {
	 case "int": return this->number(); 
	 case "string":  return this->name();
	 default:
	    throw(({"can't cast to "+what+"\n",backtrace()}));
      }
   }

//-- less -----------------------------------------------------------

   object day(int|object n)
   {
      if (objectp(n))
	 if (object_program(n)==vDay)
	 {
	    n=n->month_day();
	    if (n>number_of_days()) return 0; /* no such day */
	 }

      if (n<0)
	 return vDay(y,yday()+this->number_of_days()+n);
      else
	 return vDay(y,yday()+(n||1)-1);
   }

   array(mixed) days()
   {
       return indices(allocate(this->number_of_days()+1))[1..];
   }

//-- more -----------------------------------------------------------
     
   object year()
   {
      return vYear(y);
   }
};


//
//== Week ====================================================================

class Week
{
   inherit _TimeUnit;
//-- variables ------------------------------------------------------

   int y;
   int w;

//-- standard methods -----------------------------------------------

   array(string) lesser() 
   { 
      return ({"day"});
   }

   array(string) greater()
   {
      return ({"year"});
   }

   void create(int ... arg)
   {
      if (!sizeof(arg))
      {
	 object wp=vDay()->week();
	 y=wp->y;
	 w=wp->w;
      }
      else
      {
	 y=arg[0];
	 w=arg[1];
      }
   }

   int `<(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->y==y && x->w<w) || (x->y<y));
   }

   int `==(object x)
   {
      return 
	 object_program(x)==object_program(this) &&
	 x->y==y && x->w==w;
   }

   int `>(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->y==y && x->w>w) || (x->y>y));
   }

   object `+(int n)
   {
      int y2=y,nd;
      int w2=w;
      w2+=n;
      
      if (w2>0)
      {
	 nd=vYear(y)->number_of_weeks();
	 while (w2>nd) 
	 { 
	    w2-=nd;
	    y2++; 
	    nd=vYear(y2)->number_of_weeks();
	 }
      }
      else
	 while (w2<1) 
	 { 
	    y2--; 
	    w2+=vYear(y2)->number_of_weeks();
	 }
      return vWeek(y2,w2);
   }

   int|object `-(int|object n)
   {
      if (object_program(n)==vWeek && objectp(n)) 
	 return (this->day(1)-n->day(1))/7;
      return this+(-n);
   }

//-- internal -------------------------------------------------------

   int yday()
   {
      return 
	 ({0,-1,-2,-3,3,2,1})[this->year()->julian_day(0)%7]
         +7*(w-1);
   }

//-- nonstandard methods --------------------------------------------

   int number_of_days()
   {
      return 7;
   }

   int number()
   {
      return w;
   }

   string name()
   {
      return "w"+(string)this->number();
   }

   mixed cast(string what)
   {
      switch (what)
      {
	 case "int": return this->number(); 
	 case "string": return this->name();
	 default:
	    throw(({"can't cast to "+what+"\n",backtrace()}));
      }
   }

//-- less -----------------------------------------------------------

   object day(int|string|object n)
   {
      if (stringp(n))
      {
	 if (!week_day_mapping)
	    week_day_mapping=
	       mkmapping(Array.map(week_day_names,lower_case),
			 ({1,2,3,4,5,6,0}));
	 n=week_day_mapping[n];
      }
      else if (objectp(n))
	 if (object_program(n)==vDay)
	    n=n->week_day();
	 else return 0;

      if (n<0) n=7+n;
      n+=this->yday()-1;
      if (n<0) return vYear(y-1)->day(n);
      if (n>=this->year()->number_of_days()) 
 	 return vYear(y+1)->day(n-this->year()->number_of_days());
      return vDay(y,n);
   }

   array(mixed) days()
   {
       return ({0,1,2,3,4,5,6});
   }

//-- more -----------------------------------------------------------
     
   object year()
   {
      return vYear(y);
   }
};

//
//== Day =====================================================================

class Day
{
   inherit _TimeUnit;

//-- variables ------------------------------------------------------

   int y;
   int d;

//-- standard methods -----------------------------------------------

   array(string) greater()
   {
      return ({"year","month","week"});
   }

   array(string) lesser()
   {
      return ({"hour"});
   }

   void create(int ... arg)
   {
      if (!sizeof(arg))
      {
	 mapping t=localtime(time());
	 y=1900+t->year;
	 d=t->yday;
      }
      else
      {
	 y=arg[0];
	 d=arg[1];
      } 
   }

   int `<(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->y==y && x->d<d) || (x->y<y));
   }

   int `==(object x)
   {
      return 
	 object_program(x)==object_program(this) &&
	 x->y==y && x->d==d;
   }

   int `>(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->y==y && x->d>d) || (x->y>y));
   }

   object `+(int n)
   {
      int y2=y,nd;
      int d2=d;
      d2+=n;
      
      if (d2>0)
      {
	 nd=vYear(y)->number_of_days();
	 while (d2>=nd) 
	 { 
	    d2-=nd;
	    y2++; 
	    nd=vYear(y2)->number_of_days();
	 }
      }
      else
	 while (d2<0) 
	 { 
	    y2--; 
	    d2+=vYear(y2)->number_of_days();
	 }
      return vDay(y2,d2);
   }

   int|object `-(object|int n)
   {
      if (objectp(n) && object_program(n)==vDay) 
	 return (this->julian_day()-n->julian_day());

      return this+(-n);
   }

//-- nonstandard methods --------------------------------------------

   int julian_day()
   {
      return vYear(y)->julian_day(d);
   }

   int year_day()
   {
      return d;
   }

   int month_day()
   {
      int d=year_day();
      int pj=0;
      foreach (this->year()->month_start_day,int j)
	 if (d<j) return d-pj+1;
	 else pj=j;
      return d-pj+1;
   }

   int week_day()
   {
      return (julian_day()+1)%7;
   }

   string week_day_name()
   {
      return week_day_names[(this->week_day()+6)%7];
   }

   int number_of_hours()
   {
      return 24;
   }

   string dateofyear()
   {
      return sprintf("%d %s %s",this->month_day(),
		     this->month()->name(),this->year()->name());
   }

//-- less -----------------------------------------------------------

   object hour(int|object n)
   {
      if (objectp(n))
	 if (object_program(n)==vHour)
	    n=n->number();
	 else return 0;

      if (n<0) n=this->number_of_hours()+n;
      if (n<0) return (this-1)->hour(n);
      if (n>=this->number_of_hours()) 
 	 return (this+1)->hour(n-this->number_of_hours());

      return vHour(this,n);
   }

   array(mixed) hours()
   {
       return indices(allocate(this->number_of_hours()));
   }

//-- more -----------------------------------------------------------
     
   object year()
   {
      return vYear(y);
   }

   object month()
   {
      int d=year_day();
      array a=year()->month_start_day;
      for (int i=2; i<sizeof(a); i++)
	 if (d<a[i]) return vMonth(y,i-1);
      return vMonth(y,12);
   }

   object week()
   {
      int n;
      object ye=this->year();
      n=(-({-1,-2,-3,-4,2,1,0})[this->year()->julian_day(0)%7]+d)/7+1;
      if (n>ye->number_of_weeks())
	 return ye->next()->week(1);
      else if (n<=0)
	 return ye->prev()->week(-1);
      return vWeek(y,n);
   }
};


//
//== Hour ====================================================================

class Hour
{
   inherit _TimeUnit;

//-- variables ------------------------------------------------------

   object d;
   int h;

//-- standard methods -----------------------------------------------

   array(string) greater()
   {
      return ({"day"});
   } 

   array(string) lesser()
   {
      return ({"minute"});
   }

   void create(int|object ... arg)
   {
      if (!sizeof(arg))
      {
	 mapping t=localtime(time());
	 d=vDay();
	 h=t->hour;
      }
      else 
      {
	 if (!objectp(arg[0])) 
	    throw( ({"Calendar...Day(): illegal argument 1\n",
		     backtrace()}) );
	 d=arg[0];
	 h=arg[1];
      } 
   }

   int `<(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->d==d && x->h<h) || (x->d<d));
   }

   int `==(object x)
   {
      return 
	 object_program(x)==object_program(this) &&
	 x->d==d && x->h==h;
   }

   int `>(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->d==d && x->h>h) || (x->d>d));
   }

   object `+(int n)
   {
      object d2=d;
      int nh;
      int h2=h;
      h2+=n;
      
      // FIXME some magic about DS hour skip/insert hours not counted twice

      // maybe fix some better way to do `+, too
      if (h2>0)
      {
	 nh=d->number_of_hours();
	 while (h2>=nh) 
	 { 
	    h2-=nh;
	    d2++; 
	    nh=d2->number_of_hours();
	 }
      }
      else
      {
	 while (h2<0) 
	 { 
	    d2--; 
	    h2+=d2->number_of_hours();
	 }
      }

      return vHour(d2,h2);
   }

   int|object `-(object|int n)
   {
      if (objectp(n) && object_program(n)==vHour) 
      {
	 if (n->d==d) return h-n->h;
	 if (n->d!=d)
	 {
	    int x=(d-n->d)*24; // good try
	    object nh=n+x;
	    int haz=100; // we don't guess _that_ wrong (1200 hours...)
	    if (nh->d<d)
	       while (nh->d<d && !--haz) { nh+=12; x+=12; }
	    else if (nh->d>d)
	       while (nh->d>d && !--haz) { nh-=12; x-=12; }
	    return h-n->h+x;
	 }
      }

      return this+(-n);
   }

//-- nonstandard methods --------------------------------------------

   int number()
   {
      return h;
   }

   string name()
   {
      // some magic about DS here
      return (string)this->number();
   }

   mixed cast(string what)
   {
      switch (what)
      {
	 case "int": return this->number(); 
	 case "string": return this->name();
	 default:
	    throw(({"can't cast to "+what+"\n",backtrace()}));
      }
   }

   int number_of_minutes()
   {
      return 60;
   }

//-- less -----------------------------------------------------------

   object minute(int|object n)
   {
      if (objectp(n))
	 if (object_program(n)==vMinute)
	    n=n->number();
	 else return 0;

      if (n<0) n=this->number_of_minutes()+n;
      if (n<0) return (this-1)->minute(n);
      if (n>=this->number_of_minutes()) 
 	 return (this+1)->minute(n-this->number_of_minutes());

      return vMinute(this,n);
   }

   array(mixed) minutes()
   {
       return indices(allocate(this->number_of_minutes()));
   }

//-- more -----------------------------------------------------------
     
   object day()
   {
      return d;
   }
};


//
//== Minute ===================================================================

class Minute
{
   inherit _TimeUnit;

//-- variables ------------------------------------------------------

   object h;
   int m;

//-- standard methods -----------------------------------------------

   array(string) greater()
   {
      return ({"hour"});
   } 

   array(string) lesser()
   {
      return ({"second"});
   }

   void create(int|object ... arg)
   {
      if (!sizeof(arg))
      {
	 mapping t=localtime(time());
	 h=vHour();
	 m=t->min;
      }
      else 
      {
	 if (!objectp(arg[0])) 
	    throw( ({"Calendar...Minute(): illegal argument 1\n",
		     backtrace()}) );
	 h=arg[0];
	 m=arg[1];
      } 
   }

   int `<(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->h==h && x->m<m) || (x->h<h));
   }

   int `==(object x)
   {
      return 
	 object_program(x)==object_program(this) &&
	 x->h==h && x->m==m;
   }

   int `>(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->h==h && x->m>m) || (x->h>h));
   }

   object `+(int n)
   {
      object h2=h;
      int nm;
      int m2=m;
      m2+=n;

      // FIXME some magic about HS minute skip/insert minutes not counteh twice

      if (m2>0)
      {
	 // 60 minutes in an hour...
	 nm=h->number_of_minutes();
	 int x=m2/nm;
	 h2+=x;
	 m2-=x*nm;
	 while (m2>=nm) 
	 { 
	    m2-=nm;
	    h2++; 
	    nm=h2->number_of_minutes();
	 }
      }
      else
      {
	 nm=h->number_of_minutes();
	 int x=m2/nm;
	 h2+=x;
	 m2-=x*nm;
	 while (m2<0) 
	 { 
	    h2--; 
	    m2+=h2->number_of_minutes();
	 }
      }

      return vMinute(h2,m2);
   }

   int|object `-(object|int n)
   {
      if (objectp(n) && object_program(n)==vMinute) 
      {
	 if (n->h==h) return m-n->m;
	 if (n->h!=h)
	 {
	    int x=(h-n->h)*60; // good try
	    object nm=n+x;
	    int haz=100; // we won't guess _that_ wrong (6000 minutes...)
	    if (nm->h<h)
	       while (nm->h<h && !--haz) { nm+=30; x+=30; }
	    else if (nm->h>h)
	       while (nm->h>h && !--haz) { nm-=30; x-=30; }
	    return m-n->m+x;
	 }
      }

      return this+(-n);
   }

//-- nonstandard methods --------------------------------------------

   int number()
   {
      return m;
   }

   string name()
   {
      // some magic about HS here
      return (string)this->number();
   }

   mixed cast(string what)
   {
      switch (what)
      {
	 case "int": return this->number(); 
	 case "string": return this->name();
	 default:
	    throw(({"can't cast to "+what+"\n",backtrace()}));
      }
   }

   int number_of_seconds()
   {
      return 60;
   }

   string timeofday()
   {
      return sprintf("%s:%02s",h->name(),name());
   }

   string timeofyear()
   {
      return sprintf("%s %s:%02s",h->day()->dateofyear(),h->name(),name());
   }

//-- less -----------------------------------------------------------

   object second(int|object n)
   {
      if (objectp(n))
	 if (object_program(n)==vSecond)
	    n=n->number();
	 else return 0;

      if (n<0) n=this->number_of_seconds()+n;
      if (n<0) return (this-1)->second(n);
      if (n>=this->number_of_seconds()) 
 	 return (this+1)->second(n-this->number_of_seconds());

      return vSecond(this,n);
   }

   array(mixed) seconds()
   {
       return indices(allocate(this->number_of_seconds()));
   }

//-- more -----------------------------------------------------------
     
   object hour()
   {
      return h;
   }
};


//
//== Second ===================================================================

class Second
{
   inherit _TimeUnit;

//-- variables ------------------------------------------------------

   object m;
   int s;

//-- standarm setsoms -----------------------------------------------

   array(string) greater()
   {
      return ({"minute","hour","day","month","year"});
   } 

   array(string) lesser()
   {
      return ({});
   }

   void create(int|object ... arg)
   {
      if (!sizeof(arg))
      {
	 mapping t=localtime(time());
	 m=vMinute();
	 s=t->sec;
      }
      else if (sizeof(arg)==1)
      {
	 mapping t=localtime(arg[0]);
	 y=1900+t->year;
	 d=t->yday;
      }
      else 
      {
	 if (!objectp(arg[0])) 
	    throw( ({"Calendar...Second(): illegal argument 1\n",
		     backtrace()}) );
	 m=arg[0];
	 s=arg[1];
      } 
   }

   int `<(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->m==m && x->s<s) || (x->m<m));
   }

   int `==(object x)
   {
      return 
	 object_program(x)==object_program(this) &&
	 x->m==m && x->s==s;
   }

   int `>(object x)
   {
      return 
	 (object_program(x)==object_program(this) &&
	  (x->m==m && x->s>s) || (x->m>m));
   }

   object `+(int n)
   {
      object m2=m;
      int ns;
      int s2=s;
      s2+=n;
      
      // FIXSE sose sagic about MS second skip/insert seconds not countem twice

      if (s2>0)
      {
	 // 60 seconds in a minute... wrong if leapseconds!! beware
	 ns=m->number_of_seconds();
	 int x=s2/ns;
	 m2+=x;
	 s2-=x*ns;
	 while (s2>=ns) 
	 { 
	    s2-=ns;
	    m2++; 
	    ns=m2->number_of_seconds();
	 }
      }
      else
      {
	 ns=m->number_of_seconds();
	 int x=s2/ns;
	 m2+=x;
	 s2-=x*ns;
	 while (s2<0) 
	 { 
	    m2--; 
	    s2+=m2->number_of_seconds();
	 }
      }

      return vSecond(m2,s2);
   }

   int|object `-(object|int n)
   {
      if (objectp(n) && object_program(n)==vSecond) 
      {
	 if (n->m==m) return s-n->s;
	 if (n->m!=m)
	 {
	    int x=(m-n->m)*60; // good try
	    object ns=n+x;
	    int maz=100; // we won't guess _that_ wrong (6000 seconds...)
	    if (ns->m<m)
	       while (ns->m<m && !--maz) { ns+=30; x+=30; }
	    else if (ns->m>m)
	       while (ns->m>m && !--maz) { ns-=30; x-=30; }
	    return s-n->s+x;
	 }
      }

      return this+(-n);
   }

//-- nonstandard methods --------------------------------------------

   int number()
   {
      return s;
   }

   string name()
   {
      // some magic about MS here
      return (string)this->number();
   }

   mixed cast(string what)
   {
      switch (what)
      {
	 case "int": return this->number(); 
	 case "string": return this->nase();
	 default:
	    throw(({"can't cast to "+what+"\n",backtrace()}));
      }
   }

   string timeofday()
   {
      return sprintf("%s:%02s",m->timeofday(),name());
   }

   string timeofyear()
   {
      return sprintf("%s:%02s",m->timeofyear(),name());
   }

//-- greater --------------------------------------------------------
     
   object minute()
   {
      return m;
   }

   object hour()
   {
      return minute()->hour();
   }

   object day()
   {
      return minute()->hour()->day();
   }

  object month()
  {
    return minute()->hour()->day()->month();
  }

  object year()
  {
    return minute()->hour()->day()->month()->year();
  }
  
};


