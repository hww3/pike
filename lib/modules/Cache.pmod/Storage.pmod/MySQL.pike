/*
 * An SQL-based storage manager
 * by Francesco Chemolli <kinkie@roxen.com>
 * (C) 2000 Roxen IS
 *
 * $Id: MySQL.pike,v 1.1 2000/08/07 17:59:57 kinkie Exp $
 *
 * This storage manager provides the means to save data to an SQL-based 
 * backend.
 * 
 * For now it's mysql only, dialectization will be added at a later time.
 * Serialization should be taken care of by the low-level SQL drivers.
 *
 * Notice: the administrator is supposed to create the database and give
 * the roxen user enough privileges to write to it. It will be care
 * of this driver to create the database itself.
 * 
 */

#define MAX_KEY_SIZE "255"
#define CREATION_QUERY "create table cache ( \
cachekey varchar(" MAX_KEY_SIZE ") not null primary key, \
atime timestamp, \
ctime timestamp, \
etime timestamp default '20361231235959', \
cost float unsigned DEFAULT 1.0 NOT NULL, \
data longblob NOT NULL \
)"

Sql.sql db;

#if 0                           // set to 1 to enable debugging
#ifdef debug
#undef debug
#endif // ifdef debug
#define debug(X...) werror("Cache.Storage.mysql: "+X);werror("\n")
#else  // 1
#ifndef debug                   // if there's a clash, let's let it show
#define debug(X...) /**/
#endif // ifndef debug
#endif // 1


//database manipulation is done externally. This class only returns
//values, with some lazy decoding.
class Data {
  inherit Cache.Data;
  private int _size;
  private string db_data;
  private mixed _data;

  void create (mapping q) {
    debug("instantiating data object from %O",q);
    db_data=q->data;
    _size=sizeof(db_data);
    atime=(int)q->atime;
    ctime=(int)q->ctime;
    etime=(int)q->etime;
    cost=(float)q->cost;
  }

  int size() {
    debug("object size requested");
    return _size;
  }

  mixed data() {
    debug("data requested");
    if (_data)
      return _data;
    debug("lazy-decoding data");
    _data=decode_value(db_data);
    db_data=0;
    return _data;
  }
}
//does MySQL support multiple outstanding resultsets?
//we'll know now.
//Notice: this will fail miserably with Sybase, for instance.
//Notice: can and will throw exceptions if anything fails.
private object(Sql.sql_result) enum_result;
int(0..0)|string first() {
  debug("first()");
  array res;
  if (enum_result)
    destruct(enum_result);
  enum_result=db->big_query("select cachekey from cache");
  return next();
}

int(0..0)|string next() {
  debug("next()");
  array res;
  if (!enum_result)
    return 0;
  if (res=enum_result->fetch_row())
    return res[0];
  enum_result=0;                // enumeration finished
  return 0;  
}

void delete(string key, void|int(0..1) hard) {
  debug("deleting value for key %s",key);
  //what happens if there's no such key? Nothing should happen...
  db->query("delete from cache where cachekey='%s'",key);
}

void set(string key, mixed value,
         void|int expire_time, void|float preciousness) {
  debug("setting value for key %s (e: %d, v: %f",key,expire_time,
        preciousness?preciousness:1.0);
  mixed err;
  db->query("delete from cache where cachekey='%s'",key);
  db->query("insert into cache (cachekey,atime,ctime,etime,cost,data) "
            "values('%s',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,%s,%s,'%s')",
            db->quote(key),
            (expire_time?"FROM_UNIXTIME("+expire_time+")":"NULL"),
            (preciousness?(string)preciousness:"1"),
            db->quote(encode_value(value)));
}

int(0..0)|Cache.Data get(string key,void|int notouch) {
  debug("getting value for key %s (nt: %d)",key,notouch);
  array(mapping) result=0;
  mixed err=0;
  catch (result=db->query("select unix_timestamp(atime) as atime,"
                          "unix_timestamp(ctime) as ctime,"
                          "unix_timestamp(etime) as etime,cost,data "
                          "from cache where cachekey='%s'",key));
  if (!result || !sizeof(result))
    return 0;
  if (!notouch)
    catch(db->query("update cache set atime=CURRENT_TIMESTAMP "
                    "where cachekey='%s'",key));
  return Data(result[0]);
}

void aget(string key,
          function(string,int(0..0)|Cache.Data:void) callback) {
  callback(key,get(key));
}



void create(string sql_url) {
  array result=0;
  mixed err=0;
  db=Sql.sql(sql_url);
  // used to determine whether there already is a DB here.
  err=catch(result=db->query("select stamp from cache_admin"));
  if (err || !sizeof(result)) {
    db->query(CREATION_QUERY);
    db->query("create table cache_admin (stamp integer primary key)");
    db->query("insert into cache_admin values ('1')");
  }
}
