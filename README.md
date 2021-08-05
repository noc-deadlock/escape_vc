# README #

gem5 code repositories for XY, West First, Adaptive West First, and Escape VC (using West First routing)

### Design Description ###
Results have been used in several papers for comparisions.<br>
Adaptive West First uses the direction which has higher number of free VCs
at downstream router whenever possible
	
   * https://mayank-parasar.github.io/website/papers/drain_hpca2020.pdf
   * https://mayank-parasar.github.io/website/papers/swap_micro2019.pdf
   * https://mayank-parasar.github.io/website/papers/bindu_nocs2019.pdf
   * https://mayank-parasar.github.io/website/papers/brownianbubble_nocs2018.pdf
   * https://mayank-parasar.github.io/website/papers/swapnoc_nocarc2017.pdf

### How to build ###
Under gem5/
* scons -j15 scons/Garnet_standalone/gem5.opt

### How to run ###

* See gem5/run_script.py
* To run: python run_script.py

### Developer ###

* Mayank Parasar (mayankparasar@gmail.com)
