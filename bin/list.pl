#!/usr/bin/perl -w
# Perl directory listing script for Krakws.
# Slightly less temporary solution.
#
# Mike Perron (2013)

use File::stat;

$working=$ENV{'web_root'};

# Find the present working directory.
$dir=`dirname "$ENV{'SCRIPT_NAME'}"`;
chomp $dir;

if($dir eq "/"){
	$dirf="$dir";
} else {
	$dirf="$dir/";
}

print<<EOF;
Content-Type: Text/html

<!DOCTYPE html>
<html><head>
	<meta charset=UTF-8>
	<title>Index of $dirf</title>
	<style type=text/css>
		body { font-family: sans-serif; }
		table { border-spacing: 0px; }
		th { text-align: left; border-bottom: solid 1px black; padding: 0 1em; }
		td { padding-right: 1em; }
		.size { text-align: right; font-family: monospace; }
		.date { font-family: monospace; }
		tr:nth-child(even) { background-color:#e0e0f0; }
		tr:hover td { background-color: #d0d0e0; }
	</style>
</head><body>
	<h2>Files located in $dirf</h2>
	<table>
		<tr>
			<th>Name</th>
			<th>Size (Bytes)</th>
			<th>Last Modified</th>
			<th>Description</th>
		</tr>
EOF

# If the directory is the root dir, just clear it.
# No other directory looks like /\/$/
$dir="" if($dir eq "/");

# Loop to generate each row.
$dirname="$working/$dir";
opendir my($dh), $dirname or die "Couldn't read directory. ($dirname): $!";
my @file=readdir $dh;
closedir $dh;
@file=sort @file;

foreach (@file){
	$sb=stat("$working/$dir/$_");
	$size=$sb->size;
	$mime=`file -b "$working/$dir/$_"`;
	chomp $mime;

	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst)=localtime($sb->mtime);
	$year+=1900;
	$modified=sprintf("%02d/%02d/%04d %d:%02d:%02d",$mon,$mday,$year,$hour,$min,$sec);


	if($mime eq "directory"){
		$filef="$_/";
		$size="";
		if($_ eq ".."){
			$mime="<b>Move Up</b>";
		} elsif($_ eq "."){
			$mime="<b>Present Directory</b>";
		}
	} else {
		$filef="$_";
	}

print <<EOF;
		<tr>
			<td><a href="$dir/$_">$filef</a></td>
			<td class=size>$size</td>
			<td class=date>$modified</td>
			<td>$mime</td>
		</tr>
EOF
}

print <<EOF;
	</table>
</body></html>
EOF
