
#include "graph.h"

import Array;
import Stdio;

private inherit "polyline.pike";
private inherit "create_graph.pike";
private inherit "create_bars.pike";
private inherit "create_pie.pike";
private inherit "create_graph.pike";

//This function sets all unset objects in diagram_data and
//checks some other stuff.
mapping(string:mixed) check_mapping(mapping(string:mixed) diagram_data, 
				    string type)
{
  mapping m=diagram_data;

  
  //This maps from mapping entry to defaulvalue
  //This may be optimized, but I don't think the zeros mather. 
  //I just wanted all indices to be here. But this will not be
  //Updated so it is a bad idea in the long run.
  mapping md=
    ([
      "type":1, //This is already checked
      "subtype":1, //This is already checked
      "drawtype":"linear", //Will be set to "2D" for pie below
      "tone":0,
      "3Ddepth":10,
      "data":({({1.0}), ({2.0}), 
	       ({4.0})}), //Will be set to something else with graph
      "labels":0, //array(string) ({xquantity, yquantity, xunit, yunit})
      "xnames":0, //array(string) ?
      "ynames":0, //array(string) ?
      "fontsize":10,
      "labelsize":0, //Default is set somewhere else
      "legendfontsize":0,
      "legend_texts":0,
      "values_for_xnames":0,
      "values_for_ynames":0,
      //xmaxvalue, xminvalue, ymaxvalue, yminvalue;
      "xsize":100,
      "ysize":100,
      "image":0,
      "legendcolor":0,
      "legendimage":0,
      "bgcolor":0,
      "gbimage":0,
      "axcolor":({0,0,0}),
      "datacolors":0,
      "backdatacolors":0,
      "textcolor":({0,0,0}),
      "labelcolor":0,
      "orient":"hor",
      "linewidth":0,
      "backlinewidth":0,
      "vertgrid":0,
      "horgrid":0,
      "gridwidth":0,
      "rotate":0,
      "center":0,
      "bw":0,
      "eng":0,
      "neng":0,
      "xmin":0,
      "ymin":0,
      "name":0,
      "namecolor":0,
      "font":Image.Font(),
      "gridcolor":({0,0,0})
    ]);
    foreach(indices(md), string i)
      if (!m[i]) m[i]=md[i];
    

    switch(type)
    {
    case "pie":  
      m->type = "pie";
      m->subtype="pie";
      md->drawtype="2D";
      break;
   case "bars":
     m->type = "bars";
     m->subtype = "box";
     m_delete( m, "drawtype" );
     break;
   case "line":
     m->type = "bars";
     m->subtype = "line";
     break;
   case "norm":
     m->type = "sumbars";
     m->subtype = "norm";
     break;
   case "graph":
     m->type = "graph";
     m->subtype = "line";
    md->data=({({1.0,1.0}),({2.0,1.0}),({2.0,2.0}),({1.5,1.5})});
     break;
   case "sumbars":
     m->type = "sumbars";
     break;
   default:
     error("This error will never happen error in Diagram.pmod.\n");
    }


  return diagram_data;
}

Image.Image pie(mapping(string:mixed) diagram_data)
{
  check_mapping(diagram_data, "pie");
  return create_pie(diagram_data)->image;
} 

Image.Image bars(mapping(string:mixed) diagram_data)
{
  check_mapping(diagram_data, "bars");
  return create_bars(diagram_data)->image;
} 

Image.Image sumbars(mapping(string:mixed) diagram_data)
{
  check_mapping(diagram_data, "sumbars");
  return create_bars(diagram_data)->image;
} 
Image.Image line(mapping(string:mixed) diagram_data)
{
  check_mapping(diagram_data, "line");
  return create_bars(diagram_data)->image;
} 

Image.Image norm(mapping(string:mixed) diagram_data)
{
  check_mapping(diagram_data, "norm");
  return create_bars(diagram_data)->image;
} 

Image.Image graph(mapping(string:mixed) diagram_data)
{
  check_mapping(diagram_data, "graph");
  return create_graph(diagram_data)->image;
} 
