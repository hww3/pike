#pike __REAL_VERSION__

import ".";

static string flat(string s)
{
   return replace(lower_case(s),
		  "�������������������������������'- "/1,
		  "aaaaaaeceeeeiiiidnoooooouuuuyty"/1+({""})*3);
}

static class _language_base
{
   inherit Ruleset.Language;

   static mapping events_translate=0;

   string translate_event(string name)
   {
      if (events_translate) return events_translate[name]||name;
      return name;
   }
}

static string roman_number(int m)
{
  string res="";
  if      (m<0)      return "["+m+"]";
  if      (m==0)     return "O";
  if      (m>100000) return "["+m+"]";
  while   (m>999)  { res+="M";  m-=1000; }
  if      (m>899)  { res+="CM"; m-=900; }
  else if (m>499)  { res+="D";  m-=500; }
  else if (m>399)  { res+="CD"; m-=400; }
  while   (m>99)   { res+="C";  m-=100; }
  if      (m>89)   { res+="XC"; m-=90; }
  else if (m>49)   { res+="L";  m-=50; }
  else if (m>39)   { res+="XL"; m-=40; }
  while   (m>9)    { res+="X";  m-=10; }
  if      (m>8)      return res+"IX";
  else if (m>4)    { res+="V";  m-=5; }
  else if (m>3)      return res+"IV";
  while   (m)      { res+="I";  m--; }
  return res;
}

static class _ymd_base
{
   inherit _language_base;

   static mapping(int:string) month_n2s;
   static mapping(int:string) month_n2ss;
   static mapping(string:int) month_s2n;
   static mapping(int:string) week_day_n2s;
   static mapping(int:string) week_day_n2ss;
   static mapping(string:int) week_day_s2n;

   string month_name_from_number(int n)
   {
      return month_n2s[n];
   }

   string month_shortname_from_number(int n)
   {
      return month_n2ss[n];
   }

   int month_number_from_name(string name)
   {
      int j=(month_s2n[name])
	 || (month_s2n[flat(name)]);
      if (!j) error("no such month of year: %O\n",name);
      return j;
   }

   string week_day_name_from_number(int n)
   {
      return week_day_n2s[n];
   }

   string week_day_shortname_from_number(int n)
   {
      return week_day_n2ss[n];
   }

   int week_day_number_from_name(string name)
   {
      int j=(week_day_s2n[name])
	 || (week_day_s2n[flat(name)]);
      if (!j) error("no such day of week: %O\n",name);
      return j;
   }

   string week_name_from_number(int n)
   {
      return sprintf("w%d",n);
   }

   int week_number_from_name(string s)
   {
      int w;
      if (sscanf(s,"w%d",w)) return w;
      if (sscanf(s,"%d",w)) return w;
      return 0;
   }

   string year_name_from_number(int y)
   {
      if (y<1) return sprintf("%d BC",1-y);
      return (string)y;
   }

   int year_number_from_name(string name)
   {
      int y;
      string x;
      if (sscanf(name,"%d%s",y,x)==1 || 
	  x=="") 
	 return y>=0?y:y+1; // "-1" == integer year 0
      switch (x)
      {
	 case "AD": case " AD": return y; 
	 case "BC": case " BC": return -y+1;
	 default:
	    error("Can't understand year.\n");
      }
   }

   string month_day_name_from_number(int d,int mnd)
   {
      return (string)d;
   }

// gregorian defaults

   string gregorian_week_day_name_from_number(int n)
   {
      return week_day_n2s[(n+5)%7+1];
   }

   string gregorian_week_day_shortname_from_number(int n)
   {
      return week_day_n2ss[(n+5)%7+1];
   }

   int gregorian_week_day_number_from_name(string name)
   {
      int j=(week_day_s2n[flat(name)]);
      if (!j) error("no such day of week: %O\n",name);
      return j%7+1;
   }

   string gregorian_year_name_from_number(int y)
   {
      if (y<1) return sprintf("%d BC",1-y);
      return sprintf("%d AD",y);
   }


// discordian defaults

   string discordian_month_name_from_number(int n)
   {
      return ({0,"Chaos","Discord","Confusion","Bureaucracy","The Aftermath"})[n];
   }

   string discordian_month_shortname_from_number(int n)
   {
      return ({0,"Chs","Dsc","Cfn","Bcy","Afm"})[n];
   }

   int discordian_month_number_from_name(string name)
   {
      return (["chaos":1,"discord":2,"confusion":3,
	       "bureaucracy":4,"the aftermath":5,
	       "chs":1,"dsc":2,"cfn":3,"bcy":4,"afm":5])
	 [flat(name)];
   }

   string discordian_week_day_shortname_from_number(int n)
   {
      return ({0,"SM","BT","PD","PP","SO","ST"})[n];
   }

   string discordian_week_day_name_from_number(int n)
   {
      return ({0,"Sweetmorn","Boomtime","Pungenday","Prickle-Prickle",
	       "Setting Orange","St. Tib's day"})[n];
   }

   int discordian_week_day_number_from_name(string name)
   {
      return (["sweetmorn":1,"boomtime":2,"pungenday":3,"prickle-prickle":4,
	       "setting orange":5,"st. tib's day":6,
	       "prickleprickle":4,"setting":5,"orange":5,"tib":6,"tibs":6,
	       "sttib":6,"sttibs":6,"saint tib's day":6,
	       "sm":1,"bt":2,"pd":3,"pp":4,"so":5,"st":6])
	 [flat(name)];
   }

   string discordian_week_name_from_number(int n)
   {
      return "w"+n;
   }

   string discordian_year_name_from_number(int y)
   {
      return (string)y;
   }

// coptic defaults

   string coptic_month_name_from_number(int n)
   {
      return ({0,"Tout","Baba","Hator","Kiahk","Toba",
	       "Amshir","Baramhat","Baramouda","Pakho",
	       "Paona","Epep","Mesra","Nasie"})[n];
   }

   string coptic_month_shortname_from_number(int n)
   {
      return ({0,"Tou","Bab","Hat","Kia","Tob",
	       "Ams","Bar","Bar","Pak",
	       "Pao","Epe","Mes","Nas"})[n];
   }

   int coptic_month_number_from_name(string name)
   {
      return (["tout":1,"baba":2,"hator":3,"kiahk":4,"toba":5,
	       "amshir":6,"baramhat":7,"baramouda":8,"pakho":9,
	       "paona":10,"epep":11,"mesra":12,"nasie":13,
	       "tou":1,"bab":2,"hat":3,"kia":4,"tob":5,
	       "ams":6,"bar":7,"bar":8,"pak":9,
	       "pao":10,"epe":11,"mes":12,"nas":13])
	 [flat(name)];
   }

   string coptic_year_name_from_number(int y)
   {
      return (string)y;
   }

   int x;

// islamic defaults

   array(string) islamic_months=
   ",Muharram,Safar,Reb�u'l-awwal,Reb�ul-�chir,"
   "Djum�da'l-�la,Djum�da'l-�chira,Redjeb,Shaab�n,Ramad�n,"
   "Schaww�l,Dhu'l-k�ada,Dhu'l-Hiddja"/",";
   array(string) islamic_shortmonths= // help! :)
   ",Muharram,Safar,Reb�u'l-awwal,Reb�ul-�chir,"
   "Djum�da'l-�la,Djum�da'l-�chira,Redjeb,Shaab�n,Ramad�n,"
   "Schaww�l,Dhu'l-k�ada,Dhu'l-Hiddja"/",";
   mapping islamic_backmonth=0;
   array(string) islamic_shortweekdays=
   ",aha,ith,thu,arb,kha,dsc,sab"/",";
   array(string) islamic_weekdays=
   ",ahad,ithnain,thul�th�,arbi�,kham�s,dschuma,sabt"/",";
   mapping islamic_backweekday=0;

   string islamic_month_name_from_number(int n)
   {
      return islamic_months[n];
   }

   string islamic_month_shortname_from_number(int n)
   {
      return islamic_shortmonths[n];
   }

   int islamic_month_number_from_name(string name)
   {
      if (!islamic_backmonth)
      {
	 islamic_backmonth=
	    mkmapping(map(islamic_months[1..],flat),
		      enumerate(12,1,1))|
	    mkmapping(map(islamic_months[1..],flat),
		      enumerate(12,1,1))|
	 (["rabi1":2,
	   "rabi2":3,
	   "djumada1":4,
	   "djumada2":5]);
      }
      
      return islamic_backmonth[`-(flat(name),"-","'"," ")];
   }

   string islamic_week_day_name_from_number(int n)
   {
      return "jaum el "+islamic_weekdays[n];
   }

   string islamic_week_day_shortname_from_number(int n)
   {
      return islamic_shortweekdays[n];
   }

   int islamic_week_day_number_from_name(string name)
   {
      if (!islamic_backweekday)
      {
	 islamic_backweekday=
	    mkmapping(map(map(islamic_weekdays[1..],flat),`-,"'","-"),
		      enumerate(7,1,1))|
	    mkmapping(map(map(islamic_weekdays[1..],flat),`-,"'","-"),
		      enumerate(7,1,1));
      }
      
      sscanf(name,"jaum el %s",name);
      return islamic_backweekday[`-(flat(name),"-","'")];
   }


   string islamic_week_name_from_number(int n)
   {
      return "w"+n;
   }

   string islamic_year_name_from_number(int y)
   {
      if (y<1) return sprintf("%d BH",1-y);
      return sprintf("%d AH",y);
   }
}

// ----------------------------------------------------------------

// this sets up the mappings from the arrays

#define SETUPSTUFF							\
      month_n2s=mkmapping(enumerate(12,1,1),month_names);		\
      month_n2ss= 							\
	 mkmapping(enumerate(12,1,1),map(month_names,predef::`[],0,2));	\
      month_s2n=							\
	 mkmapping(map(map(month_names,predef::`[],0,2),flat),	\
		   enumerate(12,1,1))					\
	 | mkmapping(map(month_names,flat),enumerate(12,1,1));	\
      week_day_n2s= mkmapping(enumerate(7,1,1),week_day_names);		\
      week_day_n2ss= mkmapping(enumerate(7,1,1),			\
			       map(week_day_names,predef::`[],0,2));	\
      week_day_s2n= 							\
	 mkmapping(map(map(week_day_names,predef::`[],0,2),flat),	\
		   enumerate(7,1,1))					\
	 | mkmapping(map(week_day_names,flat),enumerate(7,1,1))

#define SETUPSTUFF2							\
      month_n2s=mkmapping(enumerate(12,1,1),month_names);		\
      month_n2ss= 							\
	 mkmapping(enumerate(12,1,1),short_month_names);	\
      month_s2n=							\
	 mkmapping(map(short_month_names,flat),	\
		   enumerate(12,1,1))					\
	 | mkmapping(map(month_names,flat),enumerate(12,1,1));	\
      week_day_n2s= mkmapping(enumerate(7,1,1),week_day_names);		\
      week_day_n2ss= mkmapping(enumerate(7,1,1),			\
			       short_week_day_names);	\
      week_day_s2n= 							\
	 mkmapping(map(short_week_day_names,flat),	\
		   enumerate(7,1,1))					\
	 | mkmapping(map(week_day_names,flat),enumerate(7,1,1))




// ========================================================================



// now the real classes:

// this should probably be called UK_en or something:

constant cENGLISH=cISO;
constant cEN=cISO;
class cISO
{
   inherit _ymd_base;
   
   constant month_names=
   ({"January","February","March","April","May","June","July","August",
     "September","October","November","December"});

   constant week_day_names=
   ({"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"});

   void create()
   {
      SETUPSTUFF;
   }

   string week_name_from_number(int n)
   {
      return sprintf("w%d",n);
   }

   int week_number_from_name(string s)
   {
      int w;
      if (sscanf(s,"w%d",w)) return w;
      if (sscanf(s,"%d",w)) return w;
      return 0;
   }

   string year_name_from_number(int y)
   {
      if (y<1) return sprintf("%d BC",1-y);
      return (string)y;
   }
};

// swedish (note: all name as cLANG where LANG is in caps)

constant cSE_SV=cSWEDISH;
constant cSV=cSWEDISH;
class cSWEDISH
{
   inherit _ymd_base;

   static private constant month_names=
   ({"januari","februari","mars","april","maj","juni","juli","augusti",
     "september","oktober","november","december"});

   static private constant week_day_names=
   ({"m�ndag","tisdag","onsdag","torsdag",
     "fredag","l�rdag","s�ndag"});

   static mapping events_translate=
   ([
      "New Year's Day":		"Ny�rsdagen",
      "Epiphany":		"Trettondag jul",
      "King's Nameday":		"H K M Konungens namnsdag",
      "Candlemas":		"Kyndelsm�ssodagen",
      "St. Valentine":		"Alla hj�rtans dag",
      "Int. Women's Day":	"Internationella kvinnodagen",
      "Crown Princess' Nameday":"H K M Kronprinsessans namnsdag",
      "Waffle Day":		"V�ffeldagen",
      "Annunciation":		"Marie beb�delsedag",
      "Labor Day":		"F�rsta maj",
      "Sweden's Flag Day":	"Svenska flaggans dag",
      "St. John the Baptist":	"Johannes D�pares dag",
      "Crown Princess' Birthday":"H K M Kronprinsessans f�delsedag",
      "Queen's Nameday":	"H K M Drottningens namnsdag",
      "UN Day":			"FN-dagen",
      "All saints Day":		"Allhelgonadagen",
      "King's Nameday":		"H K M Konungens namnsdag",
      "King's Birthday":	"H K M Konungens f�delsedag",
      "St. Lucy":		"Luciadagen",
      "Queen's Birthday":	"H K M Drottningens f�delsedag",
      "Christmas Eve":		"Julafton",
      "Christmas Day":		"Juldagen",
      "St. Stephen":		"Annandagen",
      "New Year's Eve":		"Ny�rsafton",
      "Midsummer's Eve":	"Midsommarafton",
      "Midsummer's Day":	"Midsommardagen",
      "All Saints Day":		"Allhelgonadagen",

      "Fat Tuesday":		"Fettisdagen",
      "Palm Sunday":		"Palms�ndagen",
      "Good Friday":		"L�ngfredagen",
      "Easter Eve":		"P�skafton",
      "Easter":			"P�skdagen",
      "Easter Monday":		"Annandag p�sk",
      "Ascension":		"Kristi himmelsf�rd",
      "Pentecost Eve":		"Pingstafton",
      "Pentecost":		"Pingst",
      "Pentecost Monday":	"Annandag pingst",
      "Advent 1":		"F�rsta advent",
      "Advent 2":		"Andra advent",
      "Advent 3":		"Tredje advent",
      "Advent 4":		"Fj�rde advent",
      "Mother's Day":		"Mors dag",
      "Father's Day":		"Fars dag",

      "Summer Solstice":	"Sommarsolst�nd",
      "Winter Solstice":	"Vintersolst�nd",
      "Spring Equinox":		"V�rdagj�mning",
      "Autumn Equinox":		"H�stdagj�mning",

// not translated:
//	"Halloween"
//	"Alla helgons dag"
//	"Valborgsm�ssoafton"
   ]);

   void create()
   {
      SETUPSTUFF;
   }

   string week_name_from_number(int n)
   {
      return sprintf("v%d",n);
   }

   int week_number_from_name(string s)
   {
      if (sscanf(s,"v%d",int w)) return w;
      return ::week_number_from_name(s);
   }

   string year_name_from_number(int y)
   {
      if (y<1) return sprintf("%d fk",1-y);
      return (string)y;
   }
}

// austrian
// source: Martin Baehr <mbaehr@email.archlab.tuwien.ac.at>

constant cDE_AT=cAUSTRIAN; // this is a german dialect, appearantly
class cAUSTRIAN
{
   inherit _ymd_base;

   static private constant month_names=
      ({"j�nner","feber","m�rz","april","mai","juni","juli","august",
        "september","oktober","november","dezember"});

   static private constant week_day_names=
      ({"montag","dienstag","mittwoch","donnerstag",
        "freitag","samstag","sonntag"});

   void create()
   {
      SETUPSTUFF;
   }
}

// Welsh
// source: book

constant cCY=cWELSH;
class cWELSH
{
   inherit _ymd_base;

   static private constant month_names=
   ({"ionawr","chwefror","mawrth","ebrill","mai","mehefin",
     "gorffenaf","awst","medi","hydref","tachwedd","rhagfyr"});

   static private constant week_day_names=
   ({"Llun","Mawrth","Mercher","Iau","Gwener","Sadwrn","Sul"});

   string week_day_name_from_number(int n)
   {
      return "dydd "+::week_day_name_from_number(n);
   }

   int week_day_number_from_name(string name)
   {
      sscanf(name,"dydd %s",name);
      return week_day_number_from_name(name);
   }

   void create()
   {
      SETUPSTUFF;
   }
}

// Spanish
// Julio C�sar G�zquez <jgazquez@dld.net>

constant cES=cSPANISH;
class cSPANISH
{
   inherit _ymd_base;

   static private constant month_names=
   ({"enero","febrero","marzo","abril","mayo","junio",
     "julio","agosto","setiembre","octubre","noviembre","diciembre"});

   static private constant week_day_names=
   ({"lunes","martes","mi�rcoles","jueves",
     "viernes","s�bado","domingo"});

// contains argentina for now
   static mapping events_translate=
   ([
      "Epiphany":"D�a de Reyes", // Epifania
      "Malvinas Day":"D�a de las Malvinas",
      "Labor Day":"Aniversario de la Revoluci�n",
      "Soberany's Day":"D�a de la soberania",
      "Flag's Day":"D�a de la bandera",
      "Independence Day":"D�a de la independencia",
      "Assumption Day":"D�a de la asunci�n", // ?
      "Gral San Mart�n decease":
      "Aniversario del fallecimiento del Gral. San Martin",
      "Race's Day":"D�a de la Raza",
      "All Saints Day":"D�a de todos los santos",
      "Immaculate Conception":"Inmaculada Concepci�n",
      "Christmas Day":"Natividad del Se�or",
      "New Year's Day":"A�o Nuevo",
      "Holy Thursday":"Jueves Santo",
      "Good Friday":"Viernes Santo",
      "Holy Saturday":"S�bado de gloria",
      "Easter":"Domingo de resurrecci�n",
      "Corpus Christi":"Corpus Christi"
   ]);

   void create()
   {
      SETUPSTUFF;
   }
}

// portugese
// source: S�rgio Ara�jo <sergio@projecto-oasis.cx>

constant cPT=cPORTUGESE; // Portugese (Brasil)
class cPORTUGESE
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "Janeiro",
      "Fevereiro",
      "Mar�o",
      "Abril",
      "Maio",
      "Junho",
      "Julho",
      "Agosto",
      "Setembro",
      "Outubro",
      "Novembro",
      "Dezembro",
   });

   static private constant week_day_names=
   ({
      "Segunda-feira", // -feira is removed for the short version
      "Ter�a-feira",   // don't know how it's used
      "Quarta-feira",
      "Quinta-feira",
      "Sexta-feira",
      "S�bado",
      "Domingo",
   });

// contains argentina for now
   static mapping events_translate=
   ([
      "New Year's Day":"Ano Novo",
      "Good Friday":"Sexta-Feira Santa",
      "Liberty Day":"Dia da Liberdade",
      "Labor Day":"Dia do Trabalhador",
      "Portugal Day":"Dia de Portugal",
      "Corpus Christi":"Corpo de Deus",
      "Assumption Day":"Assun��o",
      "Republic Day":"Implanta��o da Rep�blica",
      "All Saints Day":"Todos-os-Santos",
      "Restoration of the Independence":"Restaura��o da Independ�ncia",
      "Immaculate Conception":"Imaculada Concei��o",
      "Christmas":"Natal"
   ]);

   void create()
   {
      SETUPSTUFF;
   }
}

// Hungarian
// Csongor Fagyal <concept@conceptonline.hu>

constant cHU=cHUNGARIAN;
class cHUNGARIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({"Janu�r","Febru�r","M�rcius","�prilis","M�jus","J�nius",
     "J�lius","August","September","October","November","December"});

   static private constant week_day_names=
   ({"H�tfo","Kedd","Szerda","Cs�t�rtk�k","P�ntek","Szombat","Vas�rnap"});

// contains argentina for now
   static mapping events_translate=
   ([
      "New Year's Day":"�b �v �nnepe",
      "1848 Revolution Day":"Az 'Az 1848-as Forradalom Napja",
      "Holiday of Labor":"A munka �nnepe",
      "Constitution Day":"Az alkotm�ny �nnepe",
      "'56 Revolution Day":"Az '56-os Forradalom �nnepe",
      "Easter":"H�sv�t",
      "Easter monday":"H�sv�t",
      "Whitsunday":"P�nk�sd",
      "Whitmonday":"P�nk�sd",
      "Christmas":"Christmas",
   ]);

   void create()
   {
      SETUPSTUFF;
   }
}

// Modern Latin
// source: book

constant cLA=cLATIN;
class cLATIN
{
   inherit _ymd_base;

   static array(string) month_names=
   ({"Ianuarius", "Februarius", "Martius", "Aprilis", "Maius", "Iunius", 
     "Iulius", "Augustus", "September", "October", "November", "December" });

   static private constant week_day_names=
   ({"lunae","Martis","Mercurii","Jovis","Veneris","Saturni","solis"});

   string week_day_name_from_number(int n)
   {
      return ::week_day_name_from_number(n)+" dies";
   }

   int week_day_number_from_name(string name)
   {
      sscanf(name,"%s dies",name);
      return week_day_number_from_name(name);
   }

   string gregorian_year_name_from_number(int y)
   {
      return year_name_from_number(y);
   }

   string year_name_from_number(int y)
   {
      if (y<1) return sprintf("%d BC",1-y); // ?
      return sprintf("anno ab Incarnatione Domini %s",roman_number(y));
   }

   void create()
   {
      SETUPSTUFF;
   }
}

// Roman latin
// source: calendar FAQ + book

class cROMAN
{
   inherit cLATIN;

   static array(string) month_names=
   ({"Ianuarius", "Februarius", "Martius", "Aprilis", "Maius", "Iunius",
     "Quintilis", // Iulius
     "Sextilis",  // Augustus
     "September", "October", "November", "December"
   });

   string year_name_from_number(int y)
   {
      return sprintf("%s ab urbe condita",roman_number(y+752));
   }

   int year_number_from_name(string name)
   {
      int y;
      sscanf(name,"%d",y);
      return y-752;
   }
   
   string month_day_name_from_number(int d,int mnd)
   {
// this is not really correct, I've seen but
// i can't find it - they did something like 4 from the start of the
// months, 19 from the end of the month.
      return roman_number(d); 
   }
}


// source: anonymous unix locale file

constant cKL=cGREENLANDIC; // Greenlandic 
class cGREENLANDIC
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "januari",
      "februari",
      "martsi",
      "aprili",
      "maji",
      "juni",
      "juli",
      "augustusi",
      "septemberi",
      "oktoberi",
      "novemberi",
      "decemberi",
   });

   static private constant week_day_names=
   ({
      "ataasinngorneq",
      "marlunngorneq",
      "pingasunngorneq",
      "sisamanngorneq",
      "tallimanngorneq",
      "arfininngorneq",
      "sabaat",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cIS=cICELANDIC; // Icelandic 
class cICELANDIC
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "Januar",
      "Februar",
      "Mars",
      "April",
      "Mai",
      "Juni",
      "Juli",
      "Agust",
      "September",
      "Oktober",
      "November",
      "Desember",
   });

   static private constant week_day_names=
   ({
      "Manudagur",
      "Tridjudagur",
      "Midvikudagur",
      "Fimmtudagur",
      "F�studagur",
      "Laugardagur",
      "Sunnudagur",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cFA=cPERSIAN; // Persian (Iran)
class cPERSIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "zanwyh",            // <zj><a+><n+><w+><yf><h+>
      "fwrwyh",            // <f+><w+><r+><w+><yf><h+>
      "mars",
      "awryl",
      "mh",
      "zwyn",              // <zj><w+><yH><n+>
      "zwyyh",
      "awt",
      "sptambr",
      "aktbr",
      "nwambr",
      "dsambr",
   });

   static private constant week_day_names=
   ({
      "dwsnbh",
      "shzsnbh",
      "tharsnbh",
      "pngzsnbh",
      "gmeh",
      "snbh",
      "ykzsnbh",           // <yf><kf><zwnj><sn><n+><b+><h+>
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cAF=cAFRIKAANS; // Afrikaans (South Africa)
class cAFRIKAANS
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "Januarie",
      "Februarie",
      "Maart",
      "April",
      "Mei",
      "Junie",
      "Julie",
      "Augustus",
      "September",
      "Oktober",
      "November",
      "Desember",
   });

   static private constant week_day_names=
   ({
      "Maandag",
      "Dinsdag",
      "Woensdag",
      "Donderdag",
      "Vrydag",
      "Saterdag",
      "Sondag",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cGA=cIRISH; // Irish (Gaelic?) 
class cIRISH
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "Ean�ir",
      "Feabhra",
      "M�rta",
      "Aibre�n",
      "M� na Bealtaine",
      "Meith",
      "I�il",
      "L�nasa",
      "Me�n F�mhair",
      "Deireadh F�mhair",
      "M� na Samhna",
      "M� na Nollag",
   });

   static private constant short_month_names=
   ({
      "Ean","Fea","M�r","Aib","Bea","Mei","I�i","L�n","MF�","DF�","Sam","Nol"
   });

   static private constant week_day_names=
   ({
      "D� Luain",
      "D� M�irt",
      "D� C�adaoin",
      "D�ardaoin",
      "D� hAoine",
      "D� Sathairn",
      "D� Domhnaigh",
   });

   static private constant short_week_day_names=
   ({
      "Lua","Mai","C�a","D�a","Aoi","Sat", "Dom",
   });

   void create() { SETUPSTUFF2; }
}


// source: anonymous unix locale file

constant cEU=cBASQUE; // Basque (Spain)
class cBASQUE
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "urtarrila",
      "otsaila",
      "martxoa",
      "apirila",
      "maiatza",
      "ekaina",
      "uztaila",
      "abuztua",
      "iraila",
      "urria",
      "azaroa",
      "abendua",
   });

   static private constant week_day_names=
   ({
      "astelehena",
      "asteartea",
      "asteazkena",
      "osteguna",
      "ostirala",
      "larunbata",
      "igandea",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cNO=cNORWEGIAN; // Norwegian 
class cNORWEGIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "januar",
      "februar",
      "mars",
      "april",
      "mai",
      "juni",
      "juli",
      "august",
      "september",
      "oktober",
      "november",
      "desember",
   });

   static private constant week_day_names=
   ({
      "mandag",
      "tirsdag",
      "onsdag",
      "torsdag",
      "fredag",
      "l�rdag",
      "s�ndag",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file



constant cNL=cDUTCH; // Dutch
class cDUTCH
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "januari",
      "februari",
      "maart",
      "april",
      "mei",
      "juni",
      "juli",
      "augustus",
      "september",
      "oktober",
      "november",
      "december",
   });

   static private constant week_day_names=
   ({
      "maandag",
      "dinsdag",
      "woensdag",
      "donderdag",
      "vrijdag",
      "zaterdag",
      "zondag",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cPL=cPOLISH; // Polish 
class cPOLISH
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "styczen",           // <s><t><y><c><z><e><n'>
      "luty",
      "marzec",
      "kwiecien",
      "maj",
      "czerwiec",
      "lipiec",
      "sierpien",
      "wrzesien",
      "pazdziernik",
      "listopad",
      "grudzien",
   });

   static private constant week_day_names=
   ({
      "poniedzialek",      // <p><o><n><i><e><d><z><i><a><l/><e><k>
      "wtorek",
      "sroda",
      "czwartek",
      "piatek",
      "sobota",
      "niedziela",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cTR=cTURKISH; // Turkish
class cTURKISH
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "Ocak",
      "Subat",
      "Mart",
      "Nisan",
      "Mayis",
      "Haziran",
      "Temmuz",
      "Agustos",
      "Eylul",
      "Ekim",
      "Kasim",
      "Aralik",
   });

   static private constant week_day_names=
   ({
      "Pazartesi",
      "Sali",              // <S><a><l><i.>
      "Carsamba",
      "Persembe",
      "Cuma",
      "Cumartesi",
      "Pazar",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file



constant cDE=cGERMAN; // German 
class cGERMAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "Januar",
      "Februar",
      "M�rz",
      "April",
      "Mai",
      "Juni",
      "Juli",
      "August",
      "September",
      "Oktober",
      "November",
      "Dezember",
   });

   static private constant week_day_names=
   ({
      "Montag",
      "Dienstag",
      "Mittwoch",
      "Donnerstag",
      "Freitag",
      "Samstag",
      "Sonntag",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cLV=cLATVIAN; // Latvian 
class cLATVIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "janvaris",          // <j><a><n><v><a-><r><i><s>
      "februaris",
      "marts",
      "aprilis",           // <a><p><r><i-><l><i><s>
      "maijs",
      "junijs",
      "julijs",
      "augusts",
      "septembris",
      "oktobris",
      "novembris",
      "decembris",
   });

   static private constant week_day_names=
   ({
      "pirmdiena",
      "otrdiena",
      "tresdiena",
      "ceturtdiena",
      "piektdiena",
      "sestdiena",
      "svetdiena",         // <s><v><e-><t><d><i><e><n><a>
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cFI=cFINNISH; // Finnish 
class cFINNISH
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "tammikuu",
      "helmikuu",
      "maaliskuu",
      "huhtikuu",
      "toukokuu",
      "kes�kuu",
      "hein�kuu",
      "elokuu",
      "syyskuu",
      "lokakuu",
      "marraskuu",
      "joulukuu",
   });

   static private constant week_day_names=
   ({
      "maanantai",
      "tiistai",
      "keskiviikko",
      "torstai",
      "perjantai",
      "lauantai",
      "sunnuntai",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cLT=cLITHUANIAN; // Lithuanian
class cLITHUANIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "sausio",
      "vasario",
      "kovo",
      "balandzio",
      "geguzes",           // <g><e><g><u><z<><e.><s>
      "birzelio",
      "liepos",
      "rugpjucio",         // <r><u><g><p><j><u-><c<><i><o>
      "rugsejo",
      "spalio",
      "lapkricio",
      "gruodzio",
   });

   static private constant week_day_names=
   ({
      "Pirmadienis",
      "Antradienis",
      "Treciadienis",
      "Ketvirtadienis",
      "Penktadienis",
      "Sestadienis",       // <S<><e><s<><t><a><d><i><e><n><i><s>
      "Sekmadienis",
   });

   void create() { SETUPSTUFF; }
}


// source: anonymous unix locale file

constant cET=cESTONIAN; // Estonian 
class cESTONIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "jaanuar",
      "veebruar",
      "m�rts",
      "aprill",
      "mai",
      "juuni",
      "juuli",
      "august",
      "september",
      "oktoober",
      "november",
      "detsember",
   });

   static private constant week_day_names=
   ({
      "esmasp�ev",
      "teisip�ev",
      "kolmap�ev",
      "neljap�ev",
      "reede",
      "laup�ev",
      "puhap�ev",          // <p><u:><h><a><p><a:><e><v>
   });

   void create() { SETUPSTUFF; }
}

constant cGL=cGALICIAN; // Galician (Spain)
class cGALICIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "Xaneiro",
      "Febreiro",
      "Marzo",
      "Abril",
      "Maio",
      "Xuno",
      "Xullo",
      "Agosto",
      "Setembro",
      "Outubro",
      "Novembro",
      "Decembro",
   });

   static private constant week_day_names=
   ({
      "Luns",
      "Martes",
      "M�rcores",
      "Xoves",
      "Venres",
      "Sabado",
      "Domingo",
   });

   void create() { SETUPSTUFF; }
}

constant cID=cINDONESIAN; 
class cINDONESIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "Januari",
      "Pebruari",
      "Maret",
      "April",
      "Mei",
      "Juni",
      "Juli",
      "Agustus",
      "September",
      "Oktober",
      "November",
      "Desember",
   });

   static private constant week_day_names=
   ({
      "Senin",
      "Selasa",
      "Rabu",
      "Kamis",
      "Jumat",
      "Sabtu",
      "Minggu",   
   });

   void create() { SETUPSTUFF; }
}

constant cFR=cFRENCH; // French
class cFRENCH
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "janvier",
      "f�vrier",
      "mars",
      "avril",
      "mai",
      "juin",
      "juillet",
      "aou",
      "septembre",
      "octobre",
      "novembre",
      "d�cembre",
   });

   static private constant week_day_names=
   ({
      "lundi",
      "mardi",
      "mercredi",
      "jeudi",
      "vendredi",
      "samedi",
      "dimanche",
   });

   void create() { SETUPSTUFF; }
}

constant cIT=cITALIAN; // Italian
class cITALIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "gennaio",
      "febbraio",
      "marzo",
      "aprile",
      "maggio",
      "giugno",
      "luglio",
      "agosto",
      "settembre",
      "ottobre",
      "novembre",
      "dicembre",
   });

   static private constant week_day_names=
   ({
      "lunedi",    // swizz italian: "luned�" - should I care?
      "martedi",
      "mercoledi",
      "giovedi",
      "venerdi",
      "sabato",
      "domenica",
   });

   void create() { SETUPSTUFF; }
}

constant cCA=cCATALAN; // Catalan (Catalonia)
class cCATALAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "gener",
      "febrer",
      "mar�",
      "abril",
      "maig",
      "juny",
      "juliol",
      "agost",
      "setembre",
      "octubre",
      "novembre",
      "decembre",
   });

   static private constant week_day_names=
   ({
      "dilluns",
      "dimarts",
      "dimecres",
      "dijous",
      "divendres",
      "dissabte",
      "diumenge",
   });

   void create() { SETUPSTUFF; }
}

constant cSL=cSLOVENIAN; // Slovenian
class cSLOVENIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "januar",
      "februar",
      "marec",
      "april",
      "maj",
      "juni",
      "juli",
      "avgust",
      "september",
      "oktober",
      "november",
      "december",
   });

   static private constant week_day_names=
   ({
      "ponedeljek",
      "torek",
      "sreda",
      "cetrtek",
      "petek",
      "sobota",
      "nedelja",
   });

   void create() { SETUPSTUFF; }
}

constant cFO=cFAROESE; // Faroese 
class cFAROESE
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "januar",
      "februar",
      "mars",
      "april",
      "mai",
      "juni",
      "juli",
      "august",
      "september",
      "oktober",
      "november",
      "desember",
   });

   static private constant week_day_names=
   ({
      "manadagur",
      "tysdagur",
      "mikudagur",
      "hosdagur",
      "friggjadagur",
      "leygardagur",
      "sunnudagur",
   });

   void create() { SETUPSTUFF; }
}

constant cRO=cROMANIAN; // Romanian
class cROMANIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "IANUARIE",
      "FEBRUARIE",
      "MARTIE",
      "APRILIE",
      "MAI",
      "IUNIE",
      "IULIE",
      "AUGUST",
      "SEPTEMBRIE",
      "OCTOMBRIE",
      "NOIEMBRIE",
      "DECEMBRIE",
   });

   static private constant week_day_names=
   ({
      "LUNI",
      "MARTI",             // <M><A><R><T,><I>
      "MIERCURI",
      "JOI",
      "VINERI",
      "SI",                // <S><I/>><M><B><A(><T><A(>
      "DUMINICA",          // <D><U><M><I><N><I><C><A(>
   });

   void create() { SETUPSTUFF; }
}

constant cHR=cCROATIAN; // Croatian
class cCROATIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "Sijecanj",          // <S><i><j><e><c<><a><n><j>
      "Veljaca",
      "Ozujak",
      "Travanj",
      "Svibanj",
      "Lipanj",
      "Srpanj",
      "Kolovoz",
      "Rujan",
      "Listopad",
      "Studeni",
      "Prosinac",
   });

   static private constant week_day_names=
   ({
      "Ponedjeljak",
      "Utorak",
      "Srijeda",
      "Cetvrtak",
      "Petak",
      "Subota",
      "Nedjelja",
   });

   void create() { SETUPSTUFF; }
}

constant cDA=cDANISH; // Danish 
class cDANISH
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "januar",
      "februar",
      "marts",
      "april",
      "maj",
      "juni",
      "juli",
      "august",
      "september",
      "oktober",
      "november",
      "december",
   });

   static private constant week_day_names=
   ({
      "mandag",
      "tirsdag",
      "onsdag",
      "torsdag",
      "fredag",
      "l�rdag",
      "s�ndag",
   });

   void create() { SETUPSTUFF; }
}

constant cSR=cSERBIAN; // Serbian (Yugoslavia)
class cSERBIAN
{
   inherit _ymd_base;

   static private constant month_names=
   ({
      "januar",
      "februar",
      "mart",
      "april",
      "maj",
      "jun",
      "jul",
      "avgust",
      "septembar",
      "oktobar",
      "novembar",
      "decembar",
   });

   static private constant week_day_names=
   ({
      "ponedeljak",
      "utorak",
      "sreda",
      "cetvrtak",
      "petak",
      "subota",
      "nedelja",
   });

   void create() { SETUPSTUFF; }
}



// ----------------------------------------------------------------

// find & compile language

static mapping _cache=([]);

Ruleset.Language `[](string lang)
{
   lang=upper_case(lang);
   Ruleset.Language l=_cache[lang];
   if (l) return l;
   program cl=::`[]("c"+lang);

   if (!cl) { return ([])[0]; }

   l=_cache[lang]=cl();
   
   return l;
}

/*
 ISO 639:1988 language codes

 aa Afar 
 ab Abkhazian 
 af Afrikaans 
 am Amharic 
 ar Arabic 
 as Assamese 
 ay Aymara 
 az Azerbaijani 
 ba Bashkir 
 be Byelorussian 
 bg Bulgarian 
 bh Bihari 
 bi Bislama 
 bn Bengali; Bangla 
 bo Tibetan 
 br Breton 
 ca Catalan 
 co Corsican 
 cs Czech 
 cy Welsh 
 da Danish 
 de German 
 dz Bhutani 
 el Greek 
 en English 
 eo Esperanto 
 es Spanish 
 et Estonian 
 eu Basque 
 fa Persian 
 fi Finnish 
 fj Fiji 
 fo Faroese 
 fr French 
 fy Frisian 
 ga Irish (recte Irish Gaelic) 
 gd Scots Gaelic (recte Scottish Gaelic) 
 gl Galician 
 gn Guarani 
 gu Gujarati 
 gv Manx Gaelic  
 ha Hausa 
 he Hebrew (formerly iw) 
 hi Hindi 
 hr Croatian 
 hu Hungarian 
 hy Armenian 
 ia Interlingua 
 id Indonesian (formerly in) 
 ie Interlingue 
 ik Inupiak 
 is Icelandic 
 it Italian 
 iu Inuktitut 
 ja Japanese 
 jw Javanese 
 ka Georgian 
 kk Kazakh 
 kl Greenlandic 
 km Cambodian 
 kn Kannada 
 ko Korean 
 ks Kashmiri 
 ku Kurdish 
 kw Cornish  
 ky Kirghiz 
 la Latin 
 lb Luxemburgish  
 ln Lingala 
 lo Laothian (recte Laotian) 
 lt Lithuanian 
 lv Latvian; Lettish 
 mg Malagasy 
 mi Maori 
 mk Macedonian 
 ml Malayalam 
 mn Mongolian 
 mo Moldavian 
 mr Marathi 
 ms Malay 
 mt Maltese 
 my Burmese 
 na Nauru 
 ne Nepali 
 nl Dutch 
 no Norwegian 
 oc Occitan 
 om (Afan) Oromo 
 or Oriya 
 pa Punjabi 
 pl Polish 
 ps Pashto, Pushto 
 pt Portuguese 
 qu Quechua 
 rm Rhaeto-Romance 
 rn Kirundi 
 ro Romanian 
 ru Russian 
 rw Kinyarwanda 
 sa Sanskrit 
 sd Sindhi 
 se Northern S�mi  
 sg Sangho 
 sh Serbo-Croatian 
 si Singhalese 
 sk Slovak 
 sl Slovenian 
 sm Samoan 
 sn Shona 
 so Somali 
 sq Albanian 
 sr Serbian 
 ss Siswati 
 st Sesotho 
 su Sundanese 
 sv Swedish 
 sw Swahili 
 ta Tamil 
 te Telugu 
 tg Tajik 
 th Thai 
 ti Tigrinya 
 tk Turkmen 
 tl Tagalog 
 tn Setswana 
 to Tonga 
 tr Turkish 
 ts Tsonga 
 tt Tatar 
 tw Twi 
 ug Uigur 
 uk Ukrainian 
 ur Urdu 
 uz Uzbek 
 vi Vietnamese 
 vo Volap�k 
 wo Wolof 
 xh Xhosa 
 yi Yiddish (formerly ji) 
 yo Yoruba 
 za Zhuang 
 zh Chinese 
 zu Zulu
*/
