# Host timestamp (excerpt)

Source: punktfunk `crates/host/punktfunk-host/src/gamestream/stream.rs` (~line 1850), MIT/Apache-2.0.

The GameStream-plane RTP timestamp is the host tick wall clock (90 kHz), not the game capture instant. Repeated frames (tick without a new frame) are stamped too. In the XITRIX common-c this arrives as `DECODE_UNIT.presentationTimeMs` (1 ms resolution), currently unused by Artemis.

```rust
            .wrapping_add(owed.map_or(1, |n| n as u32));
        let t_enc = tick.start.elapsed();

        // 90 kHz RTP from wall-clock so a variable capture rate stays correct.
        let ts = (self.stream_start.elapsed().as_secs_f64() * 90_000.0) as u32;
        let (aus, poll_err) = self.poll_aus(tick.start, owed.is_some());
        let t_pkt = tick.start.elapsed();
        if !self.enqueue(aus, ts) {
            return Ok(GsFlow::End);
        }
        if self.recover_stall(poll_err)? {
```
