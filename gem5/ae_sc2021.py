#!/usr/bin/env python2
import os
import subprocess
# import pdb; pdb.set_trace()
# first compile then run
binary = 'build/Garnet_standalone/gem5.opt'
os.system("scons -j15 {}".format(binary))


bench_caps=[ "BIT_ROTATION", "SHUFFLE", "TRANSPOSE" ]
bench=[ "bit_rotation", "shuffle", "transpose" ]

# bench_caps=[ "BIT_ROTATION" ]
# bench=[ "bit_rotation" ]

routing_algorithm=[ 'TABLE', 'XY', 'WestFirst', 'RANDOM', 'ADAPT_RAND', 'ADAPT_WF', 'ESCAPE_VC' ]

num_cores = [64, 256]
num_rows = [8, 16]

os.system('rm -rf ./results')
os.system('mkdir results')

out_dir = './results'
cycles = 10000
vnet = 0
tr = 1
vc_ = 4

for c in range(len(num_cores)):
	for rout_ in [1, 2, 6]:
		for b in range(len(bench)):
			print ("routing_algorithm: {} cores: {} benchmark: {} vc-{}".format(routing_algorithm[rout_], num_cores[c], bench_caps[b], vc_))

			pkt_lat = 0
			injection_rate = 0.02
			while (pkt_lat < 200.00):
				############ gem5 command-line ###########
				os.system("{0:s} -d {1:s}/{2:d}/{3:s}/{4:s}/vc-{5:d}/inj-{6:1.2f} configs/example/garnet_synth_traffic.py --topology=Mesh_XY --num-cpus={2:d} --num-dirs={2:d} --mesh-row={7:d} --inj-vnet={8:d} --network=garnet2.0 --router-latency=1 --sim-cycles={9:d} --injectionrate={6:1.2f} --synthetic={10:s} --routing-algorithm={11:d} ".format(binary, out_dir, num_cores[c], bench_caps[b], routing_algorithm[rout_], vc_, injection_rate, num_rows[c], vnet, cycles, bench[b], rout_))

				#### convert flot to string with required precision ####
				inj_rate="{:1.2f}".format(injection_rate)

				############ gem5 output-directory ##############
				output_dir=("{0:s}/{1:d}/{2:s}/{3:s}/vc-{4:d}/inj-{5:1.2f}".format(out_dir, num_cores[c], bench_caps[b], routing_algorithm[rout_], vc_, injection_rate))

				print ("output_dir: %s" %(output_dir))

				packet_latency = subprocess.check_output("grep -nri average_flit_latency  {0:s}  | sed 's/.*system.ruby.network.average_flit_latency\s*//'".format(output_dir), shell=True)

				pkt_lat = float(packet_latency)

				print ("injection_rate={1:1.2f} \t Packet Latency: {0:f} ".format(pkt_lat, injection_rate))
				injection_rate+=0.02


############### Extract results here ###############
for c in range(len(num_cores)):
	for rout_ in [1, 2, 6]:
		for b in range(len(bench)):
			print ("routing_algorithm: {} cores: {} benchmark: {} vc-{}".format(routing_algorithm[rout_], num_cores[c], bench_caps[b], vc_))

			pkt_lat = 0
			injection_rate = 0.02
			while (pkt_lat < 200.00):
				output_dir=("{0:s}/{1:d}/{2:s}/{3:s}/vc-{4:d}/inj-{5:1.2f}".format(out_dir, num_cores[c], bench_caps[b], routing_algorithm[rout_], vc_, injection_rate))

				if(os.path.exists(output_dir)):
					packet_latency = subprocess.check_output("grep -nri average_flit_latency  {0:s}  | sed 's/.*system.ruby.network.average_flit_latency\s*//'".format(output_dir), shell=True)
					pkt_lat = float(packet_latency)
					print ("injection_rate={1:1.2f} \t Packet Latency: {0:f} ".format(pkt_lat, injection_rate))
					injection_rate+=0.02

				else:
					pkt_lat = 1000
