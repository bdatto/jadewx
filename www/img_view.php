<html>
<body>
<script language="javascript">
function get_img(i) {
  let img = document.getElementById("main_img");
  let t_idx = img.src.indexOf("/img");
  if (t_idx > 0) {
    t_idx += 4;
    let time = img.src.substr(t_idx, 4);
    let hr = parseInt(time.substr(0, 2));
    let min = parseInt(time.substr(2, 2));
    if (i > 0) {
      ++min;
      if (min == 60) {
        min = 0;
        ++hr;
      }
    } else {
      --min;
      if (min < 0) {
        min = 59;
        --hr;
      }
    }
    let stime = "" + hr;
    if (stime.length < 2) {
      stime = "0" + stime;
    }
    if (min < 10) {
      stime += "0" + min;
    } else {
      stime += "" + min;
    }
    img.src = img.src.substring(0, t_idx) + stime + img.src.substring(t_idx + 4);
  }
}
</script>
<?php
  if (isset($_GET["start"])) {
    $date = substr($_GET["start"], 0, 8);
    $hr = substr($_GET["start"], 9, 2);
    $min = substr($_GET["start"], 11, 2);
    print("<img id=\"main_img\" src=\"/Images/" . $date . "/img" . $hr . $min . ".jpg\">\n");
    print("<input type=\"button\" value=\"Previous\" onclick=\"get_img(-1)\">&nbsp;<input type=\"button\" value=\"Next\" onclick=\"get_img(1)\">\n");
  } else {
    print("boo\n");
  }
?>
</body>
</html>
