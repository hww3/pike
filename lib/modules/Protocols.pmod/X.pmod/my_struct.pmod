/* my_struct.pmod
 *
 * This should replace ADT.struct at some time.
 */

#include "error.h"

class struct {     
  string buffer;
  int index;
  
  void create(void|string s)
  {
    buffer = s || "";
    index = 0;
  }

  /* Return data, without removing it */
  string contents()
  {
    buffer = buffer[index..];
    index = 0;
    return buffer;
  }
  
  void add_data(string s)
  {
    buffer += s;
  }

  string pop_data()
  {
    string res = buffer;
    create();
    return res;
  }
  
  void put_uint(int i, int len)
  {
    if (i<0)
      error("ADT.struct->put_uint: negative argument.\n");

    add_data(sprintf("%*c", len, i));
  }

  void put_var_string(string s, int len)
  {
    if ( (len <= 3) && (strlen(s) >= ({ -1, 0x100, 0x10000, 0x1000000 })[len] ))
      error("ADT.struct->put_var_String: Field overflow.\n");
    put_uint(strlen(s), len);
    add_data(s);
  }

  void put_bignum(object i, int|void len)
  {
    if (i<0)
      error("ADT.struct->put_ubignum: negative argument.\n");
    put_var_string(i->digits(256), len || 2);
  }

  void put_fix_string(string s)
  {
    add_data(s);
  }

#if 0
  void put_fix_array(array(int) data, int item_size)
  {
    foreach(data, int i)
      put_int(i, item_size);
  }

  void put_var_array(array(int) data, int item_size, int len)
  {
    put_int(sizeof(data), len);
    put_fix_array(data, item_size);
  }
#endif

  int get_uint(int len)
  {
    mixed i;
    if ( (strlen(buffer) - index) < len)
      error("ADT.struct->get_uint: no data\n");
    sscanf(buffer, "%*" + (string) index +"s%" + (string) len + "c", i);
    index += len;
    return i;
  }

  string get_fix_string(int len)
  {
    string res;
  
    if ((strlen(buffer) - index) < len)
      error("ADT.struct->get_fix_string: no data\n");
    res = buffer[index .. index + len - 1];
    index += len;
    return res;
  }

  string get_var_string(int len)
  {
    return get_fix_string(get_uint(len));
  }

  object get_bignum(int|void len)
  {
    return Gmp.mpz(get_var_string(len || 2), 256);
  }

  string get_rest()
  {
    string s = buffer[index..];
    create();
    return s;
  }

#if 0
  array(mixed) get_fix_array(int item_size, int size)
  {
    array(mixed) res = allocate(size);
    for(int i = 0; i<size; i++)
      res[i] = get_int(item_size);
    return res;
  }

  array(mixed) get_var_array(int item_size, int len)
  {
    return get_fix_array(item_size, get_int(len));
  }
#endif

  int is_empty()
  {
    return (index == strlen(buffer));
  }
}
