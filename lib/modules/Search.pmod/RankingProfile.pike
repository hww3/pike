// This file is part of Roxen Search
// Copyright � 2001 Roxen IS. All rights reserved.
//
// $Id: RankingProfile.pike,v 1.18 2001/08/08 23:08:21 js Exp $

//!
array(int) field_ranking;

//!
array(int) proximity_ranking;

//!
int cutoff;

//! @decl void create(void|int cutoff, void|array(int) proximity_ranking,@
//!       void|Search.Database.Base db, void|array(int)|mapping(string:int) field_ranking)
//! @[cutoff] defaults to 8, @[proximity_ranking] defaults to @code{({ 8, 7, 6, 5, 4, 3, 2, 1, })@}
//! and @[field_ranking] defaults to @code{({ 17, 0, 147 }) + allocate(62)@}. @[db] is only
//! needed if @[field_ranking] is provided as a mapping.
void create(void|int _cutoff, void|array(int) _proximity_ranking,
            void|Search.Database.Base db, void|array(int)|mapping(string:int) _field_ranking)
{
  field_ranking=allocate(65);

  // Set cutoff to a value > 0.
  cutoff = _cutoff || 8;
  
  if(_proximity_ranking)
    proximity_ranking = copy_value(_proximity_ranking);
  else
    proximity_ranking = ({ 8, 7, 6, 5, 4, 3, 2, 1 });

  if(_field_ranking) {
    if(mappingp(_field_ranking))
    {
      for(int i=0; i<65; i++)
	field_ranking[i]=100;
      int field_id;
      foreach(indices(_field_ranking), string field)
        if( (field_id=db->get_field_id(field, 1)) != -1 )
	  field_ranking[field_id]=_field_ranking[field];
	else
	  werror("Search.Rankingprofile: Did not find field id for %O\n", field);
    }
    else if (arrayp(_field_ranking))
      field_ranking = copy_value(_field_ranking);
  }
  else {
    field_ranking[0]=17;
    field_ranking[2]=147;
  }
}

//! Returns a copy of this object.
this_program copy() {
  return this_program(cutoff, proximity_ranking, 0, field_ranking);
}
