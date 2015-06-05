#AGIOS

An I/O Scheduling tool.

## Overview

AGIOS is an I/O scheduling tool that can be used by I/O services to manage incoming I/O requests at files level. AGIOS aims at being generic, non-invasive, and easy to use. Moreover, it offers five scheduling algorithm options, covering multiple advantages and drawbacks: aIOLi, MLF, SJF, TO, and TO-agg.

For more information and detailed instructions, please read the [AGIOS’ documentation](AGIOS_Documentation.pdf "AGIOS doc").

## Download

The tool’s source code can be obtained from [here](AGIOS-1.0.tgz "AGIOS download").
The package also includes a patch file you can apply to [OrangeFS 2.8.7](http://orangefs.org/download/ "OrangeFS download page") in order to use AGIOS to schedule requests to its data servers. 

## Publications

“AGIOS: Application-Guided I/O Scheduling for Parallel File Systems” – ICPADS 2013.

##Giving credit

AGIOS was developed in the context of the joint laboratory LICIA between the Federal University of Rio Grande do Sul (UFRGS), in Brazil, and the University of Grenoble, in France. The research was supported by CAPES-BRAZIL – under the grant 5847/11-7 – and the HPC-GA international project.

Experiments that allowed its progress were conducted on the Grid’5000 experimental test bed, being developed under the INRIA ALADDIN development action with support from CNRS, RENATER and several Universities as well as other funding bodies (see https://www.grid5000.fr).

This tool’s source code is open and you are allowed to make modifications and adjust to your purposes, as long as your version is also freely available.

If you have further questions or want to discuss modifications to the tool, please contact Francieli Zanon Boito: fzboito [at] inf [dot] ufrgs [dot] br. 
