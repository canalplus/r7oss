
printf "%-16s %-23s %-9s %-12s %-12s %-23s %-9s %-12s\n" \
	Device\ name \Demod\ name I2C\ bus I2C\ Addr\(h\) Reset\ PIO Tuner\ name I2C\ bus I2C\ Addr\(h\)
for stm_fe_demod in `find /sys/devices/platform -name "stm_fe_demod*"`
do
dev_name=`echo $stm_fe_demod | sed -n 's%.*/%%p'`
demod_name=`cat $stm_fe_demod/conf/demod_name`
demod_io_bus=`cat $stm_fe_demod/conf/demod_io_bus`
demod_io_address=`cat $stm_fe_demod/conf/demod_io_address`
reset_pio=`cat $stm_fe_demod/conf/reset_pio`
let reset_pio_port=reset_pio/8
let reset_pio_pin=reset_pio%8
reset_pio_str="PIO($reset_pio_port,$reset_pio_pin)"
tuner_name=`cat $stm_fe_demod/conf/tuner_name`
tuner_io_bus=`cat $stm_fe_demod/conf/tuner_io_bus`
tuner_io_address=`cat $stm_fe_demod/conf/tuner_io_address`
printf "%-16s %-23s %-9s %-12x %-12s %-23s %-9s %-12x\n" \
	$dev_name $demod_name $demod_io_bus $demod_io_address $reset_pio_str $tuner_name $tuner_io_bus $tuner_io_address
done
echo ""

printf "%-16s %-23s %-12s %-12s %-12s %-12s\n" \
	Device\ name Demod\ name Demux\ TSIN TS\ Conf\(h\) TS\ Clock Custom\ flags\(h\)
for stm_fe_demod in `find /sys/devices/platform -name "stm_fe_demod*"`
do
dev_name=`echo $stm_fe_demod | sed -n 's%.*/%%p'`
demod_name=`cat $stm_fe_demod/conf/demod_name`
demux_tsin_id=`cat $stm_fe_demod/conf/demux_tsin_id`
ts_out=`cat $stm_fe_demod/conf/ts_out`
ts_clock=`cat $stm_fe_demod/conf/ts_clock`
custom_flags=`cat $stm_fe_demod/conf/custom_flags`
printf "%-16s %-23s %-12s %-12s %-12s %-12s\n" \
	$dev_name $demod_name $demux_tsin_id $ts_out $ts_clock $custom_flags
done
echo ""

printf "%-16s %-23s %-10s %-14s %-10s %-12s %-10s\n" \
	Device\ name Demod\ name Tuner\ Clk Tuner\ Clk\ Div Demod\ Clk Roll\ off\(h\) Tuner\ IF
for stm_fe_demod in `find /sys/devices/platform -name "stm_fe_demod*"`
do
dev_name=`echo $stm_fe_demod | sed -n 's%.*/%%p'`
demod_name=`cat $stm_fe_demod/conf/demod_name`
clk_config_tuner_clk=`cat $stm_fe_demod/conf/clk_config_tuner_clk`
clk_config_tuner_clkout_divider=`cat $stm_fe_demod/conf/clk_config_tuner_clkout_divider`
clk_config_demod_clk=`cat $stm_fe_demod/conf/clk_config_demod_clk`
roll_off=`cat $stm_fe_demod/conf/roll_off`
tuner_if=`cat $stm_fe_demod/conf/tuner_if`
printf "%-16s %-23s %-10s %-14s %-10s %-12s %-10s\n" \
	$dev_name $demod_name $clk_config_tuner_clk $clk_config_tuner_clkout_divider $clk_config_demod_clk $roll_off $tuner_if
done
echo ""

printf "%-16s %-23s %-9s %-12s %-12s\n" \
	Device\ name LNB\ name I2C\ bus I2C\ Addr\(h\) Custom\ flags\(h\)
for stm_fe_lnb in `find /sys/devices/platform -name "stm_fe_lnb*"`
do
dev_name=`echo $stm_fe_lnb | sed -n 's%.*/%%p'`
lnb_name=`cat $stm_fe_lnb/conf/lnb_name`
lnb_io_bus=`cat $stm_fe_lnb/conf/lnb_io_bus`
lnb_io_address=`cat $stm_fe_lnb/conf/lnb_io_address`
cust_flags=`cat $stm_fe_lnb/conf/cust_flags`
printf "%-16s %-23s %-9s %-12s %-12s\n" \
	$dev_name $lnb_name $lnb_io_bus $lnb_io_address $cust_flags
done
echo ""

printf "%-16s %-23s %-12s\n" \
	Device\ name Diseqc\ name Version\(h\)
for stm_fe_diseqc in `find /sys/devices/platform -name "stm_fe_diseqc*"`
do
dev_name=`echo $stm_fe_diseqc | sed -n 's%.*/%%p'`
diseqc_name=`cat $stm_fe_diseqc/conf/diseqc_name`
ver=`cat $stm_fe_diseqc/conf/ver`
printf "%-16s %-23s %-12s\n" \
	$dev_name $diseqc_name $ver
done
echo ""



