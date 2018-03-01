open Jest;
open Wonka_types;

let it = test;

describe("source factories", () => {
  describe("fromList", () => {
    open Expect;
    open! Expect.Operators;

    it("sends list items to a puller sink", () => {
      let source = Wonka.fromList([10, 20, 30]);
      let talkback = ref((_: Wonka_types.talkbackT) => ());
      let signals = [||];

      source(signal => {
        switch (signal) {
        | Start(x) => {
          talkback := x;
          x(Pull);
        }
        | Push(_) => {
          ignore(Js.Array.push(signal, signals));
          talkback^(Pull);
        }
        | End => ignore(Js.Array.push(signal, signals))
        };
      });

      expect(signals) == [| Push(10), Push(20), Push(30), End |];
    });
  });

  describe("fromArray", () => {
    open Expect; open! Expect.Operators;

    it("sends array items to a puller sink", () => {
      let source = Wonka.fromArray([| 10, 20, 30 |]);
      let talkback = ref((_: Wonka_types.talkbackT) => ());
      let signals = ref([||]);

      source(signal => {
        switch (signal) {
        | Start(x) => {
          talkback := x;
          x(Pull);
        }
        | Push(_) => {
          signals := Array.append(signals^, [|signal|]);
          talkback^(Pull);
        }
        | End => signals := Array.append(signals^, [|signal|]);
        };
      });

      expect(signals^) == [| Push(10), Push(20), Push(30), End |];
    });

    it("does not blow up the stack when iterating something huge", () => {
      let arr = Array.make(100000, 123);
      let source = Wonka.fromArray(arr);
      let talkback = ref((_: Wonka_types.talkbackT) => ());
      let values = [||];

      source(signal => {
        switch (signal) {
        | Start(x) => {
          talkback := x;
          x(Pull);
        }
        | Push(x) => {
          ignore(Js.Array.push(x, values));
          talkback^(Pull);
        }
        | End => ()
        };
      });

      expect(Array.length(values)) == Array.length(arr);
    });
  });
});

describe("operator factories", () => {
  describe("map", () => {
    open Expect;

    it("maps all emissions of a source", () => {
      let num = ref(1);
      let nums = [||];
      let talkback = ref((_: Wonka_types.talkbackT) => ());

      Wonka.map((_) => {
        let res = num^;
        num := num^ + 1;
        res
      }, sink => {
        sink(Start(signal => {
          switch (signal) {
          | Pull => sink(Push(1));
          | _ => ()
          }
        }));
      }, signal => {
        switch (signal) {
        | Start(x) => {
          talkback := x;
          x(Pull);
        }
        | Push(x) when num^ < 6 => {
          ignore(Js.Array.push(x, nums));
          talkback^(Pull);
        }
        | _ => ()
        }
      });

      expect(nums) |> toEqual([|1, 2, 3, 4|])
    });
  });

  describe("filter", () => {
    open Expect;

    it("filters emissions according to a predicate", () => {
      let i = ref(1);
      let nums = [||];
      let talkback = ref((_: Wonka_types.talkbackT) => ());

      Wonka.filter(x => x mod 2 === 0, sink => {
        sink(Start(signal => {
          switch (signal) {
          | Pull => {
            let num = i^;
            i := i^ + 1;
            sink(Push(num));
          }
          | _ => ()
          }
        }));
      }, signal => {
        switch (signal) {
        | Start(x) => {
          talkback := x;
          x(Pull);
        }
        | Push(x) when x < 6 => {
          ignore(Js.Array.push(x, nums));
          talkback^(Pull);
        }
        | _ => ()
        }
      });

      expect(nums) |> toEqual([|2, 4|])
    });
  });

  describe("scan", () => {
    open Expect;

    it("folds emissions using an initial seed value", () => {
      let i = ref(1);
      let nums = [||];
      let talkback = ref((_: Wonka_types.talkbackT) => ());

      Wonka.scan((acc, x) => acc + x, 0, sink => {
        sink(Start(signal => {
          switch (signal) {
          | Pull => {
            let num = i^;
            i := i^ + 1;
            sink(Push(num));
          }
          | _ => ()
          }
        }));
      }, signal => {
        switch (signal) {
        | Start(x) => {
          talkback := x;
          x(Pull);
        }
        | Push(x) when x < 7 => {
          ignore(Js.Array.push(x, nums));
          talkback^(Pull);
        }
        | _ => ()
        }
      });

      expect(nums) |> toEqual([|1, 3, 6|])
    });
  });

  describe("merge", () => {
    open Expect;
    open! Expect.Operators;

    it("merges different sources into a single one", () => {
      let a = Wonka.fromList([1, 2, 3]);
      let b = Wonka.fromList([4, 5, 6]);
      let talkback = ref((_: Wonka_types.talkbackT) => ());
      let signals = [||];
      let source = Wonka.merge([| a, b |]);

      source(signal => {
        switch (signal) {
        | Start(x) => {
          talkback := x;
          x(Pull);
        }
        | Push(_) => {
          ignore(Js.Array.push(signal, signals));
          talkback^(Pull);
        }
        | End => ignore(Js.Array.push(signal, signals))
        };
      });

      expect(signals) == [| Push(1), Push(4), Push(5), Push(6), Push(2), Push(3), End |];
    });
  });

  describe("share", () => {
    open Expect;

    it("shares an underlying source with all sinks", () => {
      let talkback = ref((_: Wonka_types.talkbackT) => ());
      let num = ref(1);
      let nums = [||];

      let source = Wonka.share(sink => {
        sink(Start(signal => {
          switch (signal) {
          | Pull => {
            let i = num^;
            num := num^ + 1;
            sink(Push(i));
          }
          | _ => ()
          }
        }));
      });

      source(signal => {
        switch (signal) {
        | Start(x) => talkback := x
        | Push(x) => ignore(Js.Array.push(x, nums))
        | _ => ()
        }
      });

      source(signal => {
        switch (signal) {
        | Push(x) => ignore(Js.Array.push(x, nums))
        | _ => ()
        }
      });

      talkback^(Pull);
      let numsA = Array.copy(nums);
      talkback^(Pull);
      expect((numsA, nums)) |> toEqual(([| 1, 1 |], [| 1, 1, 2, 2 |]));
    });
  });

  describe("combine", () => {
    open Expect;

    it("combines the latest values of two sources", () => {
      let talkback = ref((_: Wonka_types.talkbackT) => ());

      let makeSource = (factor: int) => {
        let num = ref(1);

        sink => {
          sink(Start(signal => {
            switch (signal) {
            | Pull => {
              let i = num^ * factor;
              num := num^ + 1;
              sink(Push(i));
            }
            | _ => ()
            }
          }));
        }
      };

      let sourceA = makeSource(1);
      let sourceB = makeSource(2);
      let source = Wonka.combine(sourceA, sourceB);
      let res = [||];

      source(signal => {
        switch (signal) {
        | Start(x) => talkback := x
        | Push(x) => ignore(Js.Array.push(x, res))
        | _ => ()
        }
      });

      talkback^(Pull);
      talkback^(Pull);
      expect(res) |> toEqual([| (1, 2), (2, 2), (2, 4) |]);
    });
  });

  describe("take", () => {
    open Expect;

    it("only lets a maximum number of values through", () => {
      let talkback = ref((_: Wonka_types.talkbackT) => ());
      let num = ref(1);

      let source = Wonka.take(2, sink => sink(Start(signal => {
        switch (signal) {
        | Pull => {
          let i = num^;
          num := num^ + 1;
          sink(Push(i));
        }
        | _ => ()
        }
      })));

      let res = [||];

      source(signal => {
        switch (signal) {
        | Start(x) => talkback := x
        | _ => ignore(Js.Array.push(signal, res))
        }
      });

      talkback^(Pull);
      talkback^(Pull);
      talkback^(Pull);
      expect(res) |> toEqual([| Push(1), Push(2), End |]);
    });
  });
});

describe("sink factories", () => {
  describe("forEach", () => {
    open Expect;

    it("calls a function for each emission of the passed source", () => {
      let i = ref(0);
      let nums = [||];

      let source = sink => {
        sink(Start(signal => {
          switch (signal) {
          | Pull when i^ < 4 => {
            let num = i^;
            i := i^ + 1;
            sink(Push(num));
          }
          | Pull => sink(End)
          | _ => ()
          }
        }));
      };

      Wonka.forEach(x => ignore(Js.Array.push(x, nums)), source);
      expect(nums) |> toEqual([| 0, 1, 2, 3 |])
    });
  });
});

describe("chains (integration)", () => {
  open Expect;

  it("fromArray, map, forEach", () => {
    let input = Array.mapi((i, _) => i, Array.make(1000, 1));
    let output = Array.map(x => string_of_int(x));
    let actual = [||];

    input
      |> Wonka.fromArray
      |> Wonka.map(x => string_of_int(x))
      |> Wonka.forEach(x => ignore(Js.Array.push(x, actual)));

    expect(output) |> toEqual(output)
  });
});