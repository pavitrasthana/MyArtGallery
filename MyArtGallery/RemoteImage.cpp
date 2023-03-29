// Fill out your copyright notice in the Description page of Project Settings.


#include "Http.h"
#include "JsonUtilities.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "RemoteImage.h"

// Sets default values
ARemoteImage::ARemoteImage()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	SetRootComponent(SceneComponent);
	PlaneComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneCompoent"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneMesh.Succeeded())
	{
		PlaneComponent->SetStaticMesh(PlaneMesh.Object);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to Find Mesh"));
	}
	PlaneComponent->SetupAttachment(SceneComponent);

	TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextRenderComponent"));
	TextComponent->SetupAttachment(SceneComponent);

}

// Called when the game starts or when spawned
void ARemoteImage::BeginPlay()
{
	Super::BeginPlay();
	FHttpModule* HttpModule = &FHttpModule::Get();
	TSharedRef<IHttpRequest>HttpReq = HttpModule->CreateRequest();
	HttpReq->SetVerb("GET");
	FString RequestUrl = FString::Format(TEXT("https://api.artic.edu/api/v1/artworks/{0}?fields=artist_display,title,image_id"), { CatalogId });
	HttpReq->SetURL(RequestUrl);
	HttpReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	HttpReq->OnProcessRequestComplete().BindUObject(this, &ARemoteImage::OnResponseReceived);
	HttpReq->ProcessRequest();
}

void ARemoteImage::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[ARemoteImage] Response Failed"));
		return;
	}
	FString ResponseStr = Response->GetContentAsString();
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON"));
		return;
	}

	//Getting data into Data Object
	const TSharedPtr<FJsonObject, ESPMode::ThreadSafe>& DataObject = JsonObject->GetObjectField("data");
	if (!DataObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Data Object"));
		return;
	}
	FString ImageId, Title, ArtistDisplay, IIIFUrl;
	if (!TryGetStringField(DataObject, "image_id", ImageId) || !TryGetStringField(DataObject, "title", Title) ||
		!TryGetStringField(DataObject, "artist_display", ArtistDisplay))
	{
		return;
	}
	FString LabelText = FString::Format(TEXT("{0}\n{1}"), { Title,ArtistDisplay });
	FString EnDashChar = FString::Chr(0x2013);
	FString HyphenChar = FString::Chr(0x002D);
	LabelText = LabelText.Replace(*EnDashChar, *HyphenChar);

	//Setting TextComponent
	TextComponent->SetText(FText::FromString(LabelText));

	//Getting Config into ConfigObject 

	const TSharedPtr<FJsonObject, ESPMode::ThreadSafe>& ConfigObject = JsonObject->GetObjectField("config");
	if (!ConfigObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[ARemoteImage] Invalid ConfigObject"));
		return;
	}
	if (!TryGetStringField(ConfigObject, "iiif_url", IIIFUrl))
		return;

	//Making the image url
	FString ImageUrl = FString::Format(TEXT("{0}/{1}/full/{2},/0/default.jpg"), { IIIFUrl,ImageId,TextureWidth });
	UE_LOG(LogTemp, Log, TEXT("[ARemoteImage] ImageUrl %s"), *ImageUrl);


	//Making Another Request using ImageUrl
	FHttpModule* HttpModule = &FHttpModule::Get();

	TSharedRef<IHttpRequest> GetImageRequest = FHttpModule::Get().CreateRequest();
	GetImageRequest->SetVerb("GET");
	GetImageRequest->SetURL(ImageUrl);
	GetImageRequest->OnProcessRequestComplete().BindUObject(this, &ARemoteImage::OnImageDownloaded);

	GetImageRequest->ProcessRequest();





}

void ARemoteImage::OnImageDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[ARemoteImage] Failed to get Image"));
		return;
	}

	//Creating Material Instance 
	UMaterial* MaterialToInstance = LoadObject<UMaterial>(nullptr, TEXT("Material'/Game/Materials/Example.Example'"));
	UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(MaterialToInstance, nullptr);

	TArray<uint8>ImageData = Response->GetContent();
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper>ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	TArray<uint8> UncompressedBGRA;
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num()) || !ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
	{
		UE_LOG(LogTemp, Error, TEXT("[ARemoteImage] Failed to wrap image data"));
		return;
	}
	UTexture2D* Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
	Texture->CompressionSettings = TC_Default;
	Texture->SRGB = true;
	Texture->AddToRoot();

	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, UncompressedBGRA.GetData(), UncompressedBGRA.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();

	MaterialInstance->SetTextureParameterValue("TextureParameter", Texture);
	PlaneComponent->SetMaterial(0, MaterialInstance);

	float aspectration = (float)ImageWrapper->GetHeight() / (float)ImageWrapper->GetWidth();
	PlaneComponent->SetWorldScale3D(FVector(1.f, aspectration, 1.f));
}

bool ARemoteImage::TryGetStringField(const TSharedPtr<FJsonObject, ESPMode::ThreadSafe>& JsonObject, const FString& FieldName, FString& OutString) const
{
	if (JsonObject->TryGetStringField(FieldName, OutString))
	{
		UE_LOG(LogTemp, Log, TEXT("[ARemoteImage] %s : %s"), *FieldName, *OutString);
		return true;

	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[ARemoteImage] failed to get  %s"), *FieldName);
		return false;
	}

}

