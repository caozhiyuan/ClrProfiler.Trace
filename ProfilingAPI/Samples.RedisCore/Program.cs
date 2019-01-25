using System;
using System.Net.Http;
using System.Threading.Tasks;
using StackExchange.Redis;

namespace Samples.RedisCore
{
    class Program
    {
        private static readonly HttpClient HttpClient = new HttpClient();

        static void Main(string[] args)
        {
            Console.WriteLine($"Is64BitProcess:{Environment.Is64BitProcess}");

            Run().GetAwaiter().GetResult();

            Console.ReadLine();
        }

        private static async Task Run()
        {
            await RunStackExchange("StackExchange");

            await RunGetBing();
        }

        private static async Task<string> RunGetBing()
        {
            return await HttpClient.GetStringAsync("https://cn.bing.com/").ConfigureAwait(false);
        }

        private static async Task RunStackExchange(string prefix)
        {
            prefix += "StackExchange.Redis.";

            Console.WriteLine($"Testing StackExchange.Redis {prefix}");
            using (var redis = ConnectionMultiplexer.Connect("localhost,allowAdmin=true"))
            {
                redis.Configure(Console.Out);

                var db = redis.GetDatabase(1);
                await db.StringSetAsync($"{prefix}INCR", "0");

                db.StringSet($"{prefix}INCR", "0");

                var c = db.StringGet($"{prefix}INCR");

                Console.WriteLine(c);
            }
        }
    }
}
