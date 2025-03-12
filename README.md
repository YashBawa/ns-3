<!DOCTYPE html>
<html>
<body class="stackedit">
  <div class="stackedit__html"><h1 id="NS3-simulation-for-authentication-schemes-a-walkthrough">NS3 simulation for authentication schemes Basic Idea</h1>
<p>In this writeup, we will model an authentication mechanism for IoT devices.  A NS3 model is insutable for security analysis. Thus, we are not attempting to analyse the security aspect of the scheme, we only intend to measure the network  impact  (end-to-end delay, throughput, packet delivery ratio etc) of such an scheme in a deployment setting.</p>
<p>A high level description of the scheme in question:<br>
<a href="https://i.stack.imgur.com/osMTA.png"><img src="https://i.stack.imgur.com/osMTA.png" alt="enter image description here"></a></p>
<p>For our NS3 simulation we need to know the sizes  M<sub>1</sub> - M<sub>3</sub> . In the scheme in question</p>
<ul>
<li>M<sub>1</sub> = 104 Bytes</li>
<li>M<sub>2</sub> = 84 Bytes</li>
<li>M<sub>3</sub> = 84 Bytes</li>
</ul>
</body>

</html>
