using System;
using System.Threading.Tasks;
using Datadog.Trace.ClrProfiler;
using StackExchange.Redis;

namespace Samples.RedisCore
{
    class Program
    {
        static void Main(string[] args)
        {
            RunStackExchange("StackExchange").Wait();

            Console.ReadLine();
        }

        private static string Host()
        {
            return Environment.GetEnvironmentVariable("REDIS_HOST") ?? "localhost";
        }
        private static async Task RunStackExchange(string prefix)
        {
            prefix += "StackExchange.Redis.";

            Console.WriteLine($"Testing StackExchange.Redis {prefix}");
            using (var redis = ConnectionMultiplexer.Connect(Host() + ",allowAdmin=true"))
            {
                redis.Configure(Console.Out);

                var db = redis.GetDatabase(1);
                var n = await db.StringSetAsync($"{prefix}INCR", "0");
                Console.WriteLine(n);

                n = db.StringSet($"{prefix}INCR", "0");
                Console.WriteLine(n);
            }

            Console.ReadLine();
        }
    }
}
