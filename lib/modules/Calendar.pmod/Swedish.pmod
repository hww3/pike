inherit Calendar.ISO;

void create()
{
   month_names=
      ({"januari","februari","mars","april","maj","juni","juli","augusti",
	"september","oktober","november","december"});

   week_day_names=
      ({"m�ndag","tisdag","onsdag","torsdag",
	"fredag","l�rdag","s�ndag"});
}

class Week
{
   inherit Calendar.ISO.Week;

   string name()
   {
      return "v"+(string)this->number();
   }
}

class Year
{
   inherit Calendar.ISO.Year;

   string name()
   {
      if (this->number()<=0) 
	 return (string)(1-this->number())+" fk";
      return (string)this->number();
   }
}
