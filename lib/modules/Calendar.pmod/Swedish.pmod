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

   array(array(string)) _namedays;
   mapping(string:int) _nameday_lookup;

   string name()
   {
      if (this->number()<=0) 
	 return (string)(1-this->number())+" fk";
      return (string)this->number();
   }

   array(array(string)) namedays()
   {
      if (_namedays) return _namedays;

      array(array(string)) a;

      // insert test for year here
      if (!(a=namedays_cache[this->leap()+" "+this->leap_day()]))
      {
      // insert test for year here
	 a=namedays_1993;

	 if (this->leap())
	 {
	    a=a[..this->leap_day()-1]+
	       Array.map(allocate(this->leap()),
			 lambda(int x) { return ({}); })+
	       a[this->leap_day()..];
	 }

	 namedays_cache[this->leap()+" "+this->leap_day()]=a;
      }

      return _namedays=a;
   }

   object nameday(string name)
   {
      if (!_nameday_lookup
	 && !(_nameday_lookup=
	      namedays_lookup_cache[this->leap()+" "+this->leap_day()]))
      {
	 mapping m=([]);
	 int i;
	 foreach (this->namedays(),array a)
	 {
	    foreach (a,string name) m[lower_case(name)]=i;
	    i++;
	 }
	 _nameday_lookup =
	    namedays_lookup_cache[this->leap()+" "+this->leap_day()] = m;
      }

      return this->day(_nameday_lookup[lower_case(name)]);
   }
}

class Day
{
   inherit Calendar.ISO.Day;

   array(string) names()
   {
      return this->year()->namedays()[this->year_day()];
   }
}

// --- namnsdagar, data -------------------------------------------------

mapping namedays_cache=([]);
mapping namedays_lookup_cache=([]);

array(array(string)) namedays_1993= 
({ ({}), ({"Svea","Sverker"}), ({"Alfred","Alfrida"}),
   ({"Rut","Ritva"}), ({"Hanna","Hannele"}), ({"Baltsar","Kasper"}),
   ({"August","Augusta"}), ({"Erland","Erhard"}), ({"Gunnar","Gunder"}),
   ({"Sigurd","Sigmund"}), ({"Hugo","Hagar"}), ({"Frideborg","Fridolf"}),
   ({"Knut"}), ({"Felix","Felicia"}), ({"Laura","Liv"}),
   ({"Hjalmar","Hervor"}), ({"Anton","Tony"}), ({"Hilda","Hildur"}),
   ({"Henrik","Henry"}), ({"Fabian","Sebastian"}), ({"Agnes","Agneta"}),
   ({"Vincent","Veine"}), ({"Emilia","Emilie"}), ({"Erika","Eira"}),
   ({"Paul","P�l"}), ({"Bodil","Boel"}), ({"G�te","G�ta"}),
   ({"Karl","Karla"}), ({"Valter","Vilma"}), ({"Gunhild","Gunilla"}),
   ({"Ivar","Joar"}), ({"Max","Magda"}), ({"Marja","Mia"}),
   ({"Disa","Hj�rdis"}), ({"Ansgar","Anselm"}), ({"Lisa","Elise"}),
   ({"Dorotea","Dora"}), ({"Rikard","Dick"}), ({"Berta","Berthold"}),
   ({"Fanny","Betty"}), ({"Egon","Egil"}), ({"Yngve","Ingolf"}),
   ({"Evelina","Evy"}), ({"Agne","Agnar"}), ({"Valentin","Tina"}),
   ({"Sigfrid","Sigbritt"}), ({"Julia","Jill"}),
   ({"Alexandra","Sandra"}), ({"Frida","Fritz"}), ({"Gabriella","Ella"}),
   ({"Rasmus","Ruben"}), ({"Hilding","Hulda"}), ({"Marina","Marlene"}),
   ({"Torsten","Torun"}), ({"Mattias","Mats"}), ({"Sigvard","Sivert"}),
   ({"Torgny","Torkel"}), ({"Lage","Laila"}), ({"Maria","Maja"}),
   ({"Albin","Inez"}), ({"Ernst","Erna"}), ({"Gunborg","Gunvor"}),
   ({"Adrian","Ada"}), ({"Tora","Tor"}), ({"Ebba","Ebbe"}),
   ({"Isidor","Doris"}), ({"Siv","Saga"}), ({"Torbj�rn","Ambj�rn"}),
   ({"Edla","Ethel"}), ({"Edvin","Elon"}), ({"Viktoria","Viktor"}),
   ({"Greger","Iris"}), ({"Matilda","Maud"}), ({"Kristofer","Christel"}),
   ({"Herbert","Gilbert"}), ({"Gertrud","G�rel"}), ({"Edvard","Eddie"}),
   ({"Josef","Josefina"}), ({"Joakim","Kim"}), ({"Bengt","Benny"}),
   ({"Viking","Vilgot"}), ({"Gerda","Gert"}), ({"Gabriel","Rafael"}),
   ({"Mary","Marion"}), ({"Emanuel","Manne"}), ({"Ralf","Raymond"}),
   ({"Elma","Elmer"}), ({"Jonas","Jens"}), ({"Holger","Reidar"}),
   ({"Ester","Estrid"}), ({"Harald","Halvar"}), ({"Gunnel","Gun"}),
   ({"Ferdinand","Florence"}), ({"Irene","Irja"}), ({"Nanna","Nanny"}),
   ({"Vilhelm","Willy"}), ({"Irma","Mimmi"}), ({"Vanja","Ronja"}),
   ({"Otto","Ottilia"}), ({"Ingvar","Ingvor"}), ({"Ulf","Ylva"}),
   ({"Julius","Gillis"}), ({"Artur","Douglas"}), ({"Tiburtius","Tim"}),
   ({"Olivia","Oliver"}), ({"Patrik","Patricia"}), ({"Elias","Elis"}),
   ({"Valdemar","Volmar"}), ({"Olaus","Ola"}), ({"Amalia","Amelie"}),
   ({"Annika","Anneli"}), ({"Allan","Alida"}), ({"Georg","G�ran"}),
   ({"Vega","Viveka"}), ({"Markus","Mark"}), ({"Teresia","Terese"}),
   ({"Engelbrekt","Enok"}), ({"Ture Tyko"}), ({"Kennet","Kent"}),
   ({"Mariana","Marianne"}), ({"Valborg","Maj"}), ({"Filip","Filippa"}),
   ({"John","Jack"}), ({"Monika","Mona"}), ({"Vivianne","Vivan"}),
   ({"Marit","Rita"}), ({"Lilian","Lilly"}), ({"�ke","Ove"}),
   ({"Jonatan","Gideon"}), ({"Elvira","Elvy"}), ({"M�rta","M�rit"}),
   ({"Charlotta","Lotta"}), ({"Linnea","Nina"}), ({"Lillemor","Lill"}),
   ({"Sofia","Sonja"}), ({"Hilma","Hilmer"}), ({"Nore","Nora"}),
   ({"Erik","Jerker"}), ({"Majken","Majvor"}), ({"Karolina","Lina"}),
   ({"Konstantin","Conny"}), ({"Henning","Hemming"}),
   ({"Desiree","Renee"}), ({"Ivan","Yvonne"}), ({"Urban","Ursula"}),
   ({"Vilhelmina","Helmy"}), ({"Blenda","Beda"}),
   ({"Ingeborg","Borghild"}), ({"Jean","Jeanette"}),
   ({"Fritiof","Frej"}), ({"Isabella","Isa"}), ({"Rune","Runa"}),
   ({"Rutger","Roger"}), ({"Ingemar","Gudmar"}),
   ({"Solveig","Solbritt"}), ({"Bo","Boris"}), ({"Gustav","G�sta"}),
   ({"Robert","Robin"}), ({"Eivor","Elaine"}), ({"Petra","Petronella"}),
   ({"Kerstin","Karsten"}), ({"Bertil","Berit"}), ({"Eskil","Esbj�rn"}),
   ({"Aina","Eila"}), ({"H�kan","Heidi"}), ({"Margit","Mait"}),
   ({"Axel","Axelina"}), ({"Torborg","Torvald"}), ({"Bj�rn","Bjarne"}),
   ({"Germund","Jerry"}), ({"Linda","Linn"}), ({"Alf","Alva"}),
   ({"Paulina","Paula"}), ({"Adolf","Adela"}), ({"Johan","Jan"}),
   ({"David","Salomon"}), ({"Gunni","Jim"}), ({"Selma","Herta"}),
   ({"Leo","Leopold"}), ({"Petrus","Peter"}), ({"Elof","Leif"}),
   ({"Aron","Mirjam"}), ({"Rosa","Rosita"}), ({"Aurora","Adina"}),
   ({"Ulrika","Ulla"}), ({"Melker","Agaton"}), ({"Ronald","Ronny"}),
   ({"Klas","Kaj"}), ({"Kjell","Tjelvar"}), ({"J�rgen","�rjan"}),
   ({"Anund","Gunda"}), ({"Eleonora","Ellinor"}), ({"Herman","Hermine"}),
   ({"Joel","Judit"}), ({"Folke","Odd"}), ({"Ragnhild","Ragnvald"}),
   ({"Reinhold","Reine"}), ({"Alexis","Alice"}), ({"Fredrik","Fred"}),
   ({"Sara","Sally"}), ({"Margareta","Greta"}), ({"Johanna","Jane"}),
   ({"Magdalena","Madeleine"}), ({"Emma","Emmy"}),
   ({"Kristina","Stina"}), ({"Jakob","James"}), ({"Jesper","Jessika"}),
   ({"Marta","Moa"}), ({"Botvid","Seved"}), ({"Olof","Olle"}),
   ({"Algot","Margot"}), ({"Elin","Elna"}), ({"Per","Pernilla"}),
   ({"Karin","Kajsa"}), ({"Tage","Tanja"}), ({"Arne","Arnold"}),
   ({"Ulrik","Alrik"}), ({"Sixten","S�lve"}), ({"Dennis","Donald"}),
   ({"Silvia","Sylvia"}), ({"Roland","Roine"}), ({"Lars","Lorentz"}),
   ({"Susanna","Sanna"}), ({"Klara","Clary"}), ({"Hillevi","Gullvi"}),
   ({"William","Bill"}), ({"Stella","Stefan"}), ({"Brynolf","Sigyn"}),
   ({"Verner","Veronika"}), ({"Helena","Lena"}), ({"Magnus","M�ns"}),
   ({"Bernhard","Bernt"}), ({"Jon","Jonna"}), ({"Henrietta","Henny"}),
   ({"Signe","Signhild"}), ({"Bartolomeus","Bert"}),
   ({"Lovisa","Louise"}), ({"�sten","Ejvind"}), ({"Rolf","Rudolf"}),
   ({"Gurli","Gull"}), ({"Hans","Hampus"}), ({"Albert","Albertina"}),
   ({"Arvid","Vidar"}), ({"Samuel","Sam"}), ({"Justus","Justina"}),
   ({"Alfhild","Alfons"}), ({"Gisela","Glenn"}), ({"Harry","Harriet"}),
   ({"Sakarias","Esaias"}), ({"Regina","Roy"}), ({"Alma","Ally"}),
   ({"Anita","Anja"}), ({"Tord","Tove"}), ({"Dagny","Daniela"}),
   ({"Tyra","�sa"}), ({"Sture","Styrbj�rn"}), ({"Ida","Ellida"}),
   ({"Sigrid","Siri"}), ({"Dag","Daga"}), ({"Hildegard","Magnhild"}),
   ({"Alvar","Orvar"}), ({"Fredrika","Carita"}), ({"Agda","Agata"}),
   ({"Ellen","Elly"}), ({"Maurits","Morgan"}), ({"Tekla","Tea"}),
   ({"Gerhard","Gert"}), ({"K�re","Tryggve"}), ({"Einar","Enar"}),
   ({"Dagmar","Rigmor"}), ({"Lennart","Leonard"}),
   ({"Mikael","Mikaela"}), ({"Helge","Helny"}), ({"Ragnar","Ragna"}),
   ({"Ludvig","Louis"}), ({"Evald","Osvald"}), ({"Frans","Frank"}),
   ({"Bror","Bruno"}), ({"Jenny","Jennifer"}), ({"Birgitta","Britta"}),
   ({"Nils","Nelly"}), ({"Ingrid","Inger"}), ({"Helmer","Hadar"}),
   ({"Erling","Jarl"}), ({"Valfrid","Ernfrid"}), ({"Birgit","Britt"}),
   ({"Manfred","Helfrid"}), ({"Hedvig","Hedda"}), ({"Fingal","Finn"}),
   ({"Antonia","Annette"}), ({"Lukas","Matteus"}), ({"Tore","Torleif"}),
   ({"Sibylla","Camilla"}), ({"Birger","B�rje"}), ({"Marika","Marita"}),
   ({"S�ren","Severin"}), ({"Evert","Eilert"}), ({"Inga","Ingvald"}),
   ({"Amanda","My"}), ({"Sabina","Ina"}), ({"Simon","Simone"}),
   ({"Viola","Vivi"}), ({"Elsa","Elsie"}), ({"Edit","Edgar"}),
   ({"Andre","Andrea"}), ({"Tobias","Toini"}), ({"Hubert","Diana"}),
   ({"Uno","Unn"}), ({"Eugen","Eugenia"}), ({"Gustav Adolf"}),
   ({"Ingegerd","Ingela"}), ({"Vendela","Vanda"}), ({"Teodor","Ted"}),
   ({"Martin","Martina"}), ({"M�rten"}), ({"Konrad","Kurt"}),
   ({"Kristian","Krister"}), ({"Emil","Mildred"}), ({"Katja","Nadja"}),
   ({"Edmund","Gudmund"}), ({"Naemi","Nancy"}), ({"Pierre","Percy"}),
   ({"Elisabet","Lisbeth"}), ({"Pontus","Pia"}), ({"Helga","Olga"}),
   ({"Cecilia","Cornelia"}), ({"Klemens","Clarence"}),
   ({"Gudrun","Runar"}), ({"Katarina","Carina"}), ({"Linus","Love"}),
   ({"Astrid","Asta"}), ({"Malte","Malkolm"}), ({"Sune","Synn�ve"}),
   ({"Anders","Andreas"}), ({"Oskar","Ossian"}), ({"Beata","Beatrice"}),
   ({"Lydia","Carola"}), ({"Barbro","Barbara"}), ({"Sven","Svante"}),
   ({"Nikolaus","Niklas"}), ({"Angelika","Angela"}),
   ({"Virginia","Vera"}), ({"Anna","Annie"}), ({"Malin","Malena"}),
   ({"Daniel","Dan"}), ({"Alexander","Alex"}), ({"Lucia"}),
   ({"Sten","Stig"}), ({"Gottfrid","Gotthard"}), ({"Assar","Astor"}),
   ({"Inge","Ingemund"}), ({"Abraham","Efraim"}), ({"Isak","Rebecka"}),
   ({"Israel","Moses"}), ({"Tomas","Tom"}), ({"Natanael","Natalia"}),
   ({"Adam"}), ({"Eva"}), ({}), ({"Stefan","Staffan"}),
   ({"Johannes","Hannes"}), ({"Abel","Set"}), ({"Gunl�g","�sl�g"}),
   ({"Sylvester"}), });
