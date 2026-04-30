<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US">
<context>
    <name>HelpPage</name>
    <message>
        <location filename="../UI/HelpPage.cpp" line="25"/>
        <source>
    &lt;h1&gt;High Speed Traffic Generator manual page&lt;/h1&gt;

    &lt;h2&gt;General Look&lt;/h2&gt;
    &lt;p&gt;Generator Window has 4 main blocks:&lt;/p&gt;
    &lt;ul&gt;
    &lt;li&gt;Parameters Block on the right;&lt;/li&gt;
    &lt;li&gt;Info Block in the centre;&lt;/li&gt;
    &lt;li&gt;Graph Info Block on the left;&lt;/li&gt;
    &lt;li&gt;Control Block under the Graph Block.&lt;/li&gt;
    &lt;/ul&gt;

    &lt;h2&gt;Control Block under Graph Block.&lt;/h2&gt;
    &lt;p&gt;Control Block consists of two buttons and a time selector.&lt;br&gt;
    First button is the Start/Stop button (has a media-player-like Play icon when the generator is not active and Stop icon when active). Used to start and stop generating.&lt;br&gt;
    Second is the Pause/Resume button. Used to pause generating and resume it.&lt;br&gt;&lt;br&gt;
    &lt;b&gt;Note!&lt;/b&gt; When the Generator is active, all options in the Parameters Block and time selector are frozen.&lt;br&gt;
    Right of the Pause/Resume button is the Time Selector. When time is set to 0, the Generator will generate continuous traffic until the time hits 23:59:59 (or 1 day of generating). When stopped, the Time Selector will show total time of generating.&lt;br&gt;
    When time is set before Start, time begins to count down to 0, and for this time, Generator will create traffic.
    &lt;/p&gt;

    &lt;h2&gt;Parameters Block gives you the opportunity to set generating parameters, such as:&lt;/h2&gt;
    &lt;ul&gt;
    &lt;li&gt;&lt;b&gt;Interface&lt;/b&gt; – Name of the device which will be used for traffic to flow.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Mode&lt;/b&gt; – There are 3 modes:&lt;br&gt;
    &#x2003;1) PF_RING_STANDARD – uses default PF_RING to generate traffic. Make sure &lt;code&gt;pfring.ko&lt;/code&gt; is loaded in your system.&lt;br&gt;
    &#x2003;2) PF_RING_ZC – a more advanced way to generate faster traffic. Not all devices support it. If on install you used &lt;code&gt;-DLOAD_DRIVER=TRUE&lt;/code&gt;, you’ll have a directory with ZC driver and need to load it using &lt;code&gt;./load_driver.sh&lt;/code&gt; script.&lt;br&gt;
    &#x2003;3) DPDK – advanced way to generate traffic directly alongside the kernel. Requires DPDK preset before running the program.
    &lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Preferred Speed&lt;/b&gt; – Speed of output traffic.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Packet Size&lt;/b&gt; – Size of output packets. Can be from 16B to 64KB.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Burst Size&lt;/b&gt; – For ZC and DPDK, size of a burst of packets that will be sent together at the same time. Can be from 1 to 512.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Copies&lt;/b&gt; – Total copies that will be sent until generator stops. If set to 0, it will generate an infinite amount of copies. Max value is 18,446,744,073,709,551,615.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Sent&lt;/b&gt; – Total amount of bytes that will be sent until generator stops. If set to 0, it will generate an infinite amount. Max value is 18,446,744,073,709,551,615.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Send File button&lt;/b&gt; – If active, will send contents of file as packets (e.g. if file has 1024 bytes and packet size is 512, it will send 2 packets).&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Packet Pattern&lt;/b&gt; – Fill packet data in Hex format.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Path to file&lt;/b&gt; – Path to a &lt;code&gt;.txt&lt;/code&gt;, &lt;code&gt;.bin&lt;/code&gt; or &lt;code&gt;.hex&lt;/code&gt; file. Use the folder icon button to select.&lt;/li&gt;
    &lt;/ul&gt;

    &lt;p&gt;&lt;b&gt;IMPORTANT NOTE!&lt;/b&gt; Preferred speed does NOT mean that output speed will exactly match the value. Possible reasons for lower speed:&lt;/p&gt;
    &lt;ul&gt;
    &lt;li&gt;Too low packet size – small packets require more loops, which reduces speed.&lt;/li&gt;
    &lt;li&gt;Too low burst size – same logic as packet size, applicable to ZC and DPDK.&lt;/li&gt;
    &lt;li&gt;Hardware limit – network device can&apos;t send such volume or HugePages can&apos;t handle it.&lt;/li&gt;
    &lt;li&gt;Control handling – generator performs time checks and state comparisons which take time.&lt;/li&gt;
    &lt;/ul&gt;

    &lt;h2&gt;Graph and Info Blocks&lt;/h2&gt;
    &lt;p&gt;The Graph Block shows traffic load in bits and bytes (switchable).&lt;br&gt;
    The Info Block shows dynamic values (e.g., speed, packets per second) and static values (total copies sent, total data size sent).&lt;/p&gt;

    &lt;h2&gt;Menu Options&lt;/h2&gt;
    &lt;ul&gt;
    &lt;li&gt;&lt;b&gt;File&lt;/b&gt; – lets you:
    &#x2003;&lt;ul&gt;
    &#x2003;&lt;li&gt;Refresh interfaces – useful when network devices have changed.&lt;/li&gt;
    &#x2003;&lt;li&gt;Save graphs (bps or BPS) as an image.&lt;/li&gt;
    &#x2003;&lt;/ul&gt;
    &lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Settings&lt;/b&gt; – lets you:
    &#x2003;&lt;ul&gt;
    &#x2003;&lt;li&gt;Change the application theme (light or dark).&lt;/li&gt;
    &#x2003;&lt;li&gt;Select application language (currently supports Ukrainian and English).&lt;/li&gt;
    &#x2003;&lt;/ul&gt;
    &lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Help&lt;/b&gt; – Opens this page.&lt;/li&gt;
    &lt;/ul&gt;
    </source>
        <translation>
        &lt;h1&gt;High Speed Traffic Generator manual page&lt;/h1&gt;

    &lt;h2&gt;General Look&lt;/h2&gt;
    &lt;p&gt;Generator Window has 4 main blocks:&lt;/p&gt;
    &lt;ul&gt;
    &lt;li&gt;Parameters Block on the right;&lt;/li&gt;
    &lt;li&gt;Info Block in the centre;&lt;/li&gt;
    &lt;li&gt;Graph Info Block on the left;&lt;/li&gt;
    &lt;li&gt;Control Block under the Graph Block.&lt;/li&gt;
    &lt;/ul&gt;

    &lt;h2&gt;Control Block under Graph Block.&lt;/h2&gt;
    &lt;p&gt;Control Block consists of two buttons and a time selector.&lt;br&gt;
    First button is the Start/Stop button (has a media-player-like Play icon when the generator is not active and Stop icon when active). Used to start and stop generating.&lt;br&gt;
    Second is the Pause/Resume button. Used to pause generating and resume it.&lt;br&gt;&lt;br&gt;
    &lt;b&gt;Note!&lt;/b&gt; When the Generator is active, all options in the Parameters Block and time selector are frozen.&lt;br&gt;
    Right of the Pause/Resume button is the Time Selector. When time is set to 0, the Generator will generate continuous traffic until the time hits 23:59:59 (or 1 day of generating). When stopped, the Time Selector will show total time of generating.&lt;br&gt;
    When time is set before Start, time begins to count down to 0, and for this time, Generator will create traffic.
    &lt;/p&gt;

    &lt;h2&gt;Parameters Block gives you the opportunity to set generating parameters, such as:&lt;/h2&gt;
    &lt;ul&gt;
    &lt;li&gt;&lt;b&gt;Interface&lt;/b&gt; – Name of the device which will be used for traffic to flow.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Mode&lt;/b&gt; – There are 3 modes:&lt;br&gt;
    &#x2003;1) PF_RING_STANDARD – uses default PF_RING to generate traffic. Make sure &lt;code&gt;pfring.ko&lt;/code&gt; is loaded in your system.&lt;br&gt;
    &#x2003;2) PF_RING_ZC – a more advanced way to generate faster traffic. Not all devices support it. If on install you used &lt;code&gt;-DLOAD_DRIVER=TRUE&lt;/code&gt;, you’ll have a directory with ZC driver and need to load it using &lt;code&gt;./load_driver.sh&lt;/code&gt; script.&lt;br&gt;
    &#x2003;3) DPDK – advanced way to generate traffic directly alongside the kernel. Requires DPDK preset before running the program.
    &lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Preferred Speed&lt;/b&gt; – Speed of output traffic.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Packet Size&lt;/b&gt; – Size of output packets. Can be from 16B to 64KB.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Burst Size&lt;/b&gt; – For ZC and DPDK, size of a burst of packets that will be sent together at the same time. Can be from 1 to 512.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Copies&lt;/b&gt; – Total copies that will be sent until generator stops. If set to 0, it will generate an infinite amount of copies. Max value is 18,446,744,073,709,551,615.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Sent&lt;/b&gt; – Total amount of bytes that will be sent until generator stops. If set to 0, it will generate an infinite amount. Max value is 18,446,744,073,709,551,615.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Send File button&lt;/b&gt; – If active, will send contents of file as packets (e.g. if file has 1024 bytes and packet size is 512, it will send 2 packets).&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Packet Pattern&lt;/b&gt; – Fill packet data in Hex format.&lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Path to file&lt;/b&gt; – Path to a &lt;code&gt;.txt&lt;/code&gt;, &lt;code&gt;.bin&lt;/code&gt; or &lt;code&gt;.hex&lt;/code&gt; file. Use the folder icon button to select.&lt;/li&gt;
    &lt;/ul&gt;

    &lt;p&gt;&lt;b&gt;IMPORTANT NOTE!&lt;/b&gt; Preferred speed does NOT mean that output speed will exactly match the value. Possible reasons for lower speed:&lt;/p&gt;
    &lt;ul&gt;
    &lt;li&gt;Too low packet size – small packets require more loops, which reduces speed.&lt;/li&gt;
    &lt;li&gt;Too low burst size – same logic as packet size, applicable to ZC and DPDK.&lt;/li&gt;
    &lt;li&gt;Hardware limit – network device can&apos;t send such volume or HugePages can&apos;t handle it.&lt;/li&gt;
    &lt;li&gt;Control handling – generator performs time checks and state comparisons which take time.&lt;/li&gt;
    &lt;/ul&gt;

    &lt;h2&gt;Graph and Info Blocks&lt;/h2&gt;
    &lt;p&gt;The Graph Block shows traffic load in bits and bytes (switchable).&lt;br&gt;
    The Info Block shows dynamic values (e.g., speed, packets per second) and static values (total copies sent, total data size sent).&lt;/p&gt;

    &lt;h2&gt;Menu Options&lt;/h2&gt;
    &lt;ul&gt;
    &lt;li&gt;&lt;b&gt;File&lt;/b&gt; – lets you:
    &#x2003;&lt;ul&gt;
    &#x2003;&lt;li&gt;Refresh interfaces – useful when network devices have changed.&lt;/li&gt;
    &#x2003;&lt;li&gt;Save graphs (bps or BPS) as an image.&lt;/li&gt;
    &#x2003;&lt;/ul&gt;
    &lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Settings&lt;/b&gt; – lets you:
    &#x2003;&lt;ul&gt;
    &#x2003;&lt;li&gt;Change the application theme (light or dark).&lt;/li&gt;
    &#x2003;&lt;li&gt;Select application language (currently supports Ukrainian and English).&lt;/li&gt;
    &#x2003;&lt;/ul&gt;
    &lt;/li&gt;
    &lt;li&gt;&lt;b&gt;Help&lt;/b&gt; – Opens this page.&lt;/li&gt;
    &lt;/ul&gt;
        </translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../UI/MainWindow.cpp" line="41"/>
        <location filename="../UI/MainWindow.cpp" line="45"/>
        <location filename="../UI/MainWindow.cpp" line="49"/>
        <location filename="../UI/MainWindow.cpp" line="53"/>
        <location filename="../UI/MainWindow.cpp" line="57"/>
        <source>Input Error</source>
        <translation>Input Error</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="41"/>
        <source>Please fill in all fields.</source>
        <translation>Please fill in all fields.</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="45"/>
        <source>Please select a file to send.</source>
        <translation>Please select a file to send</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="49"/>
        <source>Packet size must be greater than 16 bytes.</source>
        <translation>Packet size must be greater than 16 bytes.</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="53"/>
        <source>Burst size must be at least 1.</source>
        <translation>Burst size must be at least 1.</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="57"/>
        <source>Please select a valid interface mode.</source>
        <translation>Please select a valid interface mode.</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="130"/>
        <source>Text Files (*.txt);;Binary Files (*.bin);;Hex Files (*.hex)</source>
        <translation>Text Files (*.txt);;Binary Files (*.bin);;Hex Files (*.hex)</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="198"/>
        <source>PF_RING Standard</source>
        <translation>PF_RING Standard</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="201"/>
        <source>PF_RING ZC</source>
        <translation>PF_RING ZC</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="204"/>
        <source>DPDK</source>
        <translation>DPDK</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="207"/>
        <source>None</source>
        <translation>None</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="447"/>
        <source>Warning</source>
        <translation>Warning</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="454"/>
        <source>Error</source>
        <translation>Error</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="528"/>
        <source>High Speed Traffic Generator</source>
        <translation>High Speed Traffic Generator</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="530"/>
        <source>Interface:</source>
        <translation>Interface:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="531"/>
        <source>Mode:</source>
        <translation>Mode:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="532"/>
        <source>Preffered Speed:</source>
        <translation>Preffered Speed:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="533"/>
        <source>Packet Size:</source>
        <translation>Packet Size:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="534"/>
        <source>Burst Size:</source>
        <translation>Burst Size:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="535"/>
        <source>Copies:</source>
        <translation>Copies:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="536"/>
        <source>Sent:</source>
        <translation>Sent:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="537"/>
        <source>Packet Pattern:</source>
        <translation>Packet Pattern:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="538"/>
        <source>Time:</source>
        <translation>Time:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="539"/>
        <source>Speed:</source>
        <translation>Speed:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="540"/>
        <source>PPS:</source>
        <translation>PPS:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="541"/>
        <source>Total Sent:</source>
        <translation>Total Sent:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="542"/>
        <source>Total Copies:</source>
        <translation>Total Copies:</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="545"/>
        <source>Send File</source>
        <translation>Send File</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="548"/>
        <source>Path to file</source>
        <translation>Path to file</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="551"/>
        <location filename="../UI/MainWindow.cpp" line="555"/>
        <source>Bits per second</source>
        <translation>Bits per second</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="552"/>
        <location filename="../UI/MainWindow.cpp" line="556"/>
        <source>Bytes per second</source>
        <translation>Bytes per second</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="559"/>
        <source>Parameters</source>
        <translation>Parameters</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="560"/>
        <source>Info</source>
        <translation>Info</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="563"/>
        <source>File</source>
        <translation>File</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="564"/>
        <source>Refresh Interfaces</source>
        <translation>Refresh Interfaces</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="565"/>
        <source>Save..</source>
        <translation>Save..</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="566"/>
        <source>Save bps Graph</source>
        <translation>Save bps Graph</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="567"/>
        <source>Save BPS Graph</source>
        <translation>Save BPS Graph</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="569"/>
        <source>Settings</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="570"/>
        <source>Theme</source>
        <translation>Theme</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="571"/>
        <source>Dark</source>
        <translation>Dark</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="572"/>
        <source>Light</source>
        <translation>Light</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="573"/>
        <source>Language</source>
        <translation>Language</translation>
    </message>
    <message>
        <location filename="../UI/MainWindow.cpp" line="578"/>
        <source>Help</source>
        <translation>Help</translation>
    </message>
</context>
</TS>
