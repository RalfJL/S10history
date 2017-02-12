# S10history

This program reads historical data from a S10 solar power station 
manufactured by [E3DC](http://www.e3dc.com) and prints it
to standard output.

Reports include:
* Energy that was feed to the battery
* Energy taken from battery
* Energy produced by solar panels
* Energy feed to the power grid
* Energy taken from the power grid
* Energy consumed by your house

## Features
* Read the sum of a Day, Month, Year
* For Day you can also list an average power (not energy) per 15 minutes
* For Month you can also list a daily energy report
* CSV output for better parsing
* PERL program to feed the output to a Mysql database
* Scripts that show some usage

## Prerequisites
* RLOG library<br>
This program uses the RLOG library for debug and error reporting.
If you do not want to use the RLOG library, you will have to replace
all rWarning, rError, ... calls with a printf call
* RSCP protocol setup in your S10 solar power station<br>
Please refer to the documentation provided by E3DC<br>
Needed: login username; login password; AES secret
* Linux like OS with a gcc
* Network connection to your S10 solar power station

## Usage
To use the S10history script you should first store the passwords
in environment variables:<br>
export PW="your login password"<br>
export AES="AES secret shared with S10"<br>
user="mys10username"<br>
ip="IP addr of S10"<br>

Reading one paticular day (2017-02-17):<br>
`S10history -u $user -P PW -A AES -i $ip -y 2017 -m 2 -d 17`

Read only the sum of that day:<br>
`S10history -u $user -P PW -A AES -i $ip -y 2017 -m 2 -d 17 -b`

Read the sum of yesterday:<br>
`S10history -u $user -P PW -A AES -i $ip  -d -1 -b`

Read the Month and all days of that month:<br>
`S10history -u $user -P PW -A AES -i $ip -y 2017 -m 2`

Read the sum of one year:<br>
`S10history -u $user -P PW -A AES -i $ip -y 2016`

Put all days of one year into a Mysql database (please fill the scripts with your values):<br>
`examples/getYearperDay.sh 2016 > Year2016perDay.txt` # reads all days to file<br>
`./S10toMysql.pl -dbname=myDBName -user=mySQLUser -password=PWofSQLuser Year2016perDay.txt`<br>
use the file to fill the database

## Issues
A lot of values, reported by the S10 solar power station are off by some percent.
If you find a flaw in my calculations please report. I am more than happy to correct that.
* The power values of a day are inaccurate. They do not match with the WUI values
* Some of the day values in monthly report are incorrect
* The sum of all days do not match the sum of a month
* the sum of all months do not match the sum of a year 

## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request :D

## History

02.2017 Initial version

## Credits

All credits go to E3DC for providing the RSCP protocol and demo that this program is based on

