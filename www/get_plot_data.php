<?php
  if (!isset($_GET["date"])) {
    die("no date specified");
  }
  else {
    $date=$_GET["date"];
    $conn=mysqli_connect("localhost","wx","wx");
    if (!$conn) {
	die("unable to connect to database");
    }
    else {
	$date_yesterday=date("Y-m-d",strtotime($date)-86400);
	if (!$result=mysqli_query($conn,"select timestamp,i_temp_out,i_windspd,i_windgust,winddir,rh,i_press,i_rain from wx.history where from_unixtime(timestamp) >= '" . $date_yesterday . " 18:01' and from_unixtime(timestamp) <= '" . $date . " 17:56' order by timestamp")) {
	  die("query error " . mysqli_error($conn));
	}
	$rowcnt=mysqli_num_rows($result);
	$row=mysqli_fetch_array($result);
	$temps="\"temps\": [ " . strval($row[1]/10.);
	$wspds="\"wspds\": [ ";
	$wspd=$row[2]/10.;
	if ($wspd < 100.) {
	  $wspds=$wspds . strval($wspd);
	}
	else {
	  $wspds=$wspds . "NaN";
	}
	$wgusts="\"wgusts\": [ ";
	$wgust=$row[3]/10.;
	if ($wgust < 100.) {
	  $wgusts=$wgusts . strval($wgust);
	}
	else {
	  $wgusts=$wgusts . "NaN";
	}
	$wdirs="\"wdirs\": [ " . $row[4];
	$rhs="\"rhs\": [ " . $row[5];
	$pressures="\"pressures\": [ " . strval(round($row[6]*0.002953,2));
	$rain=$row[7]/100.;
	$rain_days="\"rain_days\": [ " . strval($rain);
	$next_timestamp=$row[0]+300;
	for ($n=1; $n < $rowcnt; ++$n) {
	  $row=mysqli_fetch_array($result);
	  $rain+=$row[7]/100.;
	  if ($row[0] == strval($next_timestamp)) {
	    $temps=$temps . ", " . strval($row[1]/10.);
	    $wspd=$row[2]/10.;
	    if ($wspd < 100.) {
		$wspds=$wspds . ", " . strval($wspd);
	    }
	    else {
		$wspds=$wspds . ", NaN";
	    }
	    $wgust=$row[3]/10.;
	    if ($wgust < 100.) {
		$wgusts=$wgusts . ", " . strval($wgust);
	    }
	    else {
		$wgusts=$wgusts . ", NaN";
	    }
	    $wdirs=$wdirs . ", " . $row[4];
	    $rhs=$rhs . ", " . $row[5];
	    $pressures=$pressures . ", " . strval(round($row[6]*0.002953,2));
	    $rain_days=$rain_days . ", " . $rain;
	    $next_timestamp+=300;
	  }
	  else if ($row[0] > strval($next_timestamp)) {
	    $temps=$temps . ", NaN";
	    $wspds=$wspds . ", NaN";
	    $wgusts=$wgusts . ", NaN";
	    $wdirs=$wdirs . ", NaN";
	    $rhs=$rhs . ", NaN";
	    $pressures=$pressures . ", NaN";
	    $rain_days=$rain_days . ", " . $rain;
	    $next_timestamp+=300;
	  }
	}
	$temps=$temps . "]";
	$wspds=$wspds . "]";
	$wgusts=$wgusts . "]";
	$wdirs=$wdirs . "]";
	$rhs=$rhs . "]";
	$pressures=$pressures . "]";
	$rain_days=$rain_days . "]";
	print "{ " . $temps . ", " . $wspds . ", " . $wgusts . ", " . $wdirs . ", " . $rhs . ", " . $pressures . ", " . $rain_days . " }";
	mysqli_close($conn);
    }
  }
?>
