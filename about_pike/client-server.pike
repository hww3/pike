RoxenRPC �r mycket imponerande och anv�nder n�tverket. H�r �r ett
exempel med en klient och en server:

(RoxenRPC.pmod finns i roxen/server/etc/modules/)

Server:

  #!/usr/local/bin/pike

  class bonka {
     string bonk()
     {
       return "bank!\n";
     }
  }

  int main()
  {
    object rpc = RoxenRPC.Server("localhost", 31414);
    rpc->provide("dunk", bonka());
    return -17;
  }

Klient:

  #!/usr/local/bin/pike

  int main()
  {
    object rpc = RoxenRPC.Client("localhost", 31414, "dunk");
    write(rpc->bonk());
    return 0;
  }
