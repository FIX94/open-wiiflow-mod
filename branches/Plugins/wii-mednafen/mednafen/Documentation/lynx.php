<?php require("docgen.inc"); ?>

<?php BeginPage('lynx', 'Atari Lynx'); ?>

<?php BeginSection('Introduction'); ?>
<p>
Mednafen's Atari Lynx emulation is based off of <a href="http://homepage.ntlworld.com/dystopia/">Handy</a>.
</p>
<p>
Atari Lynx emulation requires the 512 Lynx boot ROM image, named as "lynxboot.img", and placed in the Mednafen base
directory.
</p>
<?php EndSection(); ?>


<?php BeginSection('Default Key Assignments'); ?>
 <table border>
  <tr><th>Key(s):</th><th>Action:</th><th>Configuration String:</th></tr>
  <tr><td>ALT + SHIFT + 1</td><td>Activate in-game input configuration process for Lynx pad.</td><td>input_config1</td></tr>
  <tr><td>F8</td><td>Rotate the screen<i>(and optionally input controlled, by setting lynx.rotateinput)</i></td><td>rotatescreen</td></tr>
 </table>
 <p>
 <table border>
  <tr><th>Key:</th><th nowrap>Action/Button:</th></tr>
  <tr><td>Keypad 2</td><td>B</td></tr>
  <tr><td>Keypad 3</td><td>A</td></tr>
  <tr><td>Keypad 4 &amp; Enter/Return</td><td>Pause</td></tr>
  <tr><td>Keypad 7</td><td>Option 1</td></tr>
  <tr><td>Keypad 1</td><td>Option 2</td></tr>
  <tr><td>W</td><td>Up</td></tr>
  <tr><td>S</td><td>Down</td></tr>
  <tr><td>A</td><td>Left</td></tr>
  <tr><td>D</td><td>Right</td></tr>
 </table>
 </p>
<?php EndSection(); ?>

<?php PrintSettings(); ?>

<?php EndPage(); ?>

