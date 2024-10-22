// $Id$

#pike 7.7

inherit Protocols.LDAP.client : orig_client;

class result
{
  inherit orig_client::result;

  protected array decode_entries (array(object) rawres)
  {
    // The "dn" field wasn't correctly utf-8 decoded in 7.6 and
    // earlier, and older apps therefore might do that themselves.
    array res = ::decode_entries (rawres);
    if (ldap_version >= 3)
      foreach (res, mapping entry)
	entry->dn[0] = string_to_utf8 (entry->dn[0]);
    return res;
  }
}

int compare (string dn, array(string) aval)
{
  return ::compare(dn, aval[0], aval[1]);
}
