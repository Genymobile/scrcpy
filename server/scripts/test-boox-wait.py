#!/usr/bin/env python3
"""Integration guards complement the executable startup state-machine fixtures."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
base = root / 'server/src/main/java/com/genymobile/scrcpy/control'
controller = (base / 'Controller.java').read_text()
process = (base / 'BooxPenProcess.java').read_text()
channel = (base / 'ControlChannel.java').read_text()
assert 'new BooxPenStartup(' in controller, 'Controller must start pen asynchronously'
start = controller.split('public void start(TerminationListener listener)', 1)[1].split('public void stop()', 1)[0]
assert start.index('booxPen.start();') < start.index('control();'), 'Control must run while startup is pending'
assert 'controlChannel.shutdownInput();' in controller, 'Fatal startup must wake recv'
inject = controller.split('private boolean injectTouch(', 1)[1]
assert inject.index('!booxPen.isReady()') < inject.index('getEventPointAndDisplayId(position)'), 'Pending pen drops before mapping'
assert 'booxPenRotation != -1 && pointerId == POINTER_ID_PEN' in inject, 'Never SDK-fallback reserved pen while pending'
assert 'booxPen.join();' in controller, 'Join startup during final cleanup'
assert 'controlSocket.shutdownInput();' in channel, 'Wake actual socket input, not just a thread flag'
constructor = process.split('public BooxPenProcess(', 1)[1].split('public synchronized void inject', 1)[0]
assert constructor.index('BooxPenInitialization.awaitIdle(') < constructor.index('new Connection('), 'Do not spawn writer before idle'
assert 'ready.join(10000)' in process, 'Keep bounded child READY handshake'
assert 'finally {' in process.split('static String readMetadata', 1)[1].split('public static void main', 1)[0], 'Discovery must clean process on cancellation'
print('BooxWaitWiring: asynchronous startup, pending pen drop, recv wake, join, pre-spawn idle and READY timeout guards passed')
