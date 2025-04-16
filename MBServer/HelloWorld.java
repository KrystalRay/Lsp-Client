// Replace the Guava Supplier import with Java's built-in Supplier
import java.util.function.Supplier;

import org.eclipse.lsp4j.jsonrpc.messages.Either;

import magpiebridge.core.IProjectService;
import magpiebridge.core.MagpieServer;
import magpiebridge.core.ServerAnalysis;
import magpiebridge.core.ServerConfiguration;
import magpiebridge.core.ToolAnalysis;
import magpiebridge.projectservice.java.JavaProjectService;
import org.eclipse.lsp4j.MessageParams;
import org.eclipse.lsp4j.MessageType;
import org.example.WukongToolAnalysis;
import com.google.gson.JsonObject;
import org.eclipse.lsp4j.DidChangeConfigurationParams;

// 添加以下新导入
import magpiebridge.core.MagpieWorkspaceService;
import com.google.gson.JsonElement;
import org.eclipse.lsp4j.jsonrpc.JsonRpcException;

/**
 * 
 * @author Linghui Luo
 *
 */
public class HelloWorld {

  public static void main(String... args) {
    // 创建一个Supplier，用于每次客户端连接时创建新的MagpieServer实例
    Supplier<MagpieServer> createServer = () -> {
      MagpieServer server = new MagpieServer(new ServerConfiguration());
      String language = "java";
      IProjectService javaProjectService = new JavaProjectService();
      server.addProjectService(language, javaProjectService);

      // ServerAnalysis myAnalysis = new SimpleServerAnalysis();
      // ServerAnalysis myAnalysis = new WukongServerAnalysis();
      // Either<ServerAnalysis, ToolAnalysis> analysis = Either.forLeft(myAnalysis);
      // server.forwardMessageToClient(
      //   new MessageParams(MessageType.Info, "Running command: "));
      // server.addAnalysis(analysis, language);

      // ToolAnalysis myToolAnalysis = new WukongToolAnalysis();
      // Either<ServerAnalysis, ToolAnalysis> toolAnalysis = Either.forRight(myToolAnalysis);
      // server.forwardMessageToClient(
      //   new MessageParams(MessageType.Info, "Running command: "));
      // server.addAnalysis(toolAnalysis, language);

        // 创建WukongToolAnalysis实例
      WukongToolAnalysis myToolAnalysis = new WukongToolAnalysis();
      Either<ServerAnalysis, ToolAnalysis> toolAnalysis = Either.forRight(myToolAnalysis);
      server.addAnalysis(toolAnalysis, language);

      // 设置配置变更处理器
      // 修改配置变更处理器部分
      server.setWorkspaceService(new MagpieWorkspaceService(server) {
          @Override
          public void didChangeConfiguration(DidChangeConfigurationParams params) {
              try {
                  Object settingsObj = params.getSettings();
                  if (settingsObj instanceof JsonElement) {
                      JsonElement settings = (JsonElement) settingsObj;
                      if (settings != null && settings.isJsonObject()) {
                          JsonObject settingsJson = settings.getAsJsonObject();
                          if (settingsJson.has("wukong") && settingsJson.get("wukong").isJsonObject()) {
                              JsonObject wukongConfig = settingsJson.getAsJsonObject("wukong");
                              if (wukongConfig.has("classpath")) {
                                  String classpath = wukongConfig.get("classpath").getAsString();
                                  myToolAnalysis.setClassPath(classpath);
                              }
                          }
                      }
                  }
              } catch (JsonRpcException | IllegalStateException e) {
                  System.err.println("处理配置变更时出错: " + e.getMessage());
              }
          }
      });
      
      
      return server;
    };
    
    // 根据命令行参数决定使用哪种通信方式
    if (args.length > 0 && args[0].equals("--socket")) {
      int port = 8080; // 默认端口
      
      // 指定端口参数
      if (args.length > 1 && args[1].equals("--port") && args.length > 2) {
        try {
          port = Integer.parseInt(args[2]);
        } catch (NumberFormatException e) {
          System.err.println("无效的端口号: " + args[2] + "，使用默认端口8080");
        }
      }
      
      System.out.println("启动Socket服务器，监听端口: " + port);
      MagpieServer.launchOnSocketPort(port, createServer);
    } else {
      // 默认使用标准输入输出
      System.out.println("启动标准输入输出服务器");
      createServer.get().launchOnStdio();
    }
  }
}
