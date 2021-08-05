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

num_cores = [16, 64, 256]
num_rows = [4, 8, 16]

# os.system('rm -rf ./results')
# os.system('mkdir results')

out_dir = './results_sat_thrpt'
cycles = 10000
vnet = 0
tr = 1
vc_ = [1, 2, 4]  # make this a list
rout_ = [1, 2, 6] # indexes into list 'routing_algorithm'
sat_thrpt = []

for c in range(len(num_cores)):
	for r in range(len(rout_)):
		for b in range(len(bench)):
			for v in range(len(vc_)):
				print ("cores: {1:d} routing_algorithm: {0:s} benchmark: {2:s} vc-{3:d}".format(routing_algorithm[rout_[r]], num_cores[c], bench_caps[b], vc_[v]))
				pkt_lat = 0
				injection_rate = 0.02
				low_load_latency = 0.0
				while (pkt_lat < 500.00):
					############ gem5 command-line ###########
					os.system("{0:s} -d {1:s}/{2:d}/{3:s}/{4:s}/vc-{5:d}/inj-{6:1.2f} configs/example/garnet_synth_traffic.py --topology=Mesh_XY --num-cpus={2:d} --num-dirs={2:d} --mesh-row={7:d} --inj-vnet={8:d} --network=garnet2.0 --router-latency=1 --sim-cycles={9:d} --injectionrate={6:1.2f} --synthetic={10:s} --routing-algorithm={11:d} ".format(binary, out_dir, num_cores[c], bench_caps[b], routing_algorithm[rout_[r]], vc_[v], injection_rate, num_rows[c], vnet, cycles, bench[b], rout_[r]))

					#### convert flot to string with required precision ####
					inj_rate="{:1.2f}".format(injection_rate)

					############ gem5 output-directory ##############
					output_dir=("{0:s}/{1:d}/{2:s}/{3:s}/vc-{4:d}/inj-{5:1.2f}".format(out_dir, num_cores[c], bench_caps[b], routing_algorithm[rout_[r]], vc_[v], injection_rate))

					print ("output_dir: %s" %(output_dir))

					packet_latency = subprocess.check_output("grep -nri average_flit_latency  {0:s}  | sed 's/.*system.ruby.network.average_flit_latency\s*//'".format(output_dir), shell=True)

					pkt_lat = float(packet_latency)

					print ("injection_rate={1:1.2f} \t Packet Latency: {0:f} ".format(pkt_lat, injection_rate))
					# Code to capture saturation throughput
					if injection_rate == 0.02:
						low_load_latency = float(pkt_lat)
					elif (float(pkt_lat) > 6.0 * float(low_load_latency)):
						sat_thrpt.append(float(injection_rate))
						break

					if float(low_load_latency) > 70.00:
						sat_thrpt.append(float(injection_rate))
						break

					injection_rate+=0.02


############### Extract results here ###############

# Print the list here
for c in range(len(num_cores)):
	for r in range(len(rout_)):
		for b in range(len(bench)):
			for v in range(len(vc_)):
				print ("cores: {1:d} routing_algorithm: {0:s} benchmark: {2:s} vc-{3:d}".format(routing_algorithm[rout_[r]], num_cores[c], bench_caps[b], vc_[v]))

				print sat_thrpt[c*(len(rout_)*len(bench)*len(vc_)) + r*(len(bench)*len(vc_)) + b*(len(vc_)) + v]
